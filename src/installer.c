#include <gtk/gtk.h>
#include <libadwaita-1/adwaita.h>
#include <archive.h>
#include <archive_entry.h>
#include <glib/gstdio.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "modlist.h"
#include "net.h"
#include "sanitizer.h"
#include "installer.h"
#include "ui.h"

#define PAGE_BTN_COUNT 4
#define PAGE_SIZE 25
#define VPK_LIMIT 99

typedef struct {
	AdwDialog *dialog;
	GtkWidget *window, *search, *results, *details, *files, *install;
	GtkWidget *status, *slots, *refresh, *sidebar;
	GtkWidget *page_btns[PAGE_BTN_COUNT], *prev_btn, *next_btn;
	json_object *catalog, *selected, *downloads;
	GArray *matches;
	GHashTable *installed;
	char *manifest;
	int current_page, total_pages, vpk_count;
	guint selection;
	gboolean closed, busy;
} Installer;

typedef struct {
	json_object *catalog;
	gint64 checked;
	char *warning;
} CatalogResult;

typedef struct {
	char *id;
	guint selection;
} DownloadTask;

typedef struct {
	char *manifest, *id;
	json_object *files;
} InstallTask;

static json_object *catalog_cache;
static gint64 catalog_checked;
static AdwDialog *install_dialog;

static void show_results(Installer *s);
static void filter_results(Installer *s);
static void start_catalog(Installer *s, gboolean refresh);

static const char *text_field(json_object *obj, const char *key) {
	const char *text = get_json_string(obj, key);
	return text ? text : "";
}

static void free_json(gpointer obj) {
	if (obj) json_object_put(obj);
}

static gboolean save_json(const char *path, json_object *obj, GError **error) {
	return g_file_set_contents(path, json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PRETTY), -1, error);
}

static void free_catalog_result(gpointer data) {
	CatalogResult *r = data;
	if (r->catalog) json_object_put(r->catalog);
	g_free(r->warning);
	g_free(r);
}

static void catalog_worker(GTask *task, gpointer source, gpointer task_data, GCancellable *cancel) {
	gboolean refresh = GPOINTER_TO_INT(task_data);
	char *dir = g_build_filename(g_get_user_cache_dir(), "ivy", NULL);
	char *path = g_build_filename(dir, "catalog.json", NULL);
	char *meta_path = g_build_filename(dir, "catalog-meta.json", NULL);
	json_object *meta = json_object_from_file(meta_path);
	CatalogResult *r = g_new0(CatalogResult, 1);
	r->catalog = json_object_from_file(path);
	if (r->catalog && !json_object_is_type(r->catalog, json_type_array)) {
		json_object_put(r->catalog);
		r->catalog = NULL;
	}
	json_object *checked;
	if (meta && json_object_object_get_ex(meta, "checked", &checked)) r->checked = json_object_get_int64(checked);
	if (refresh || !r->catalog) {
		long status = 0;
		char *etag = NULL;
		json_object *fresh = get_mod_catalog(r->catalog && meta ? text_field(meta, "etag") : NULL, &etag, &status);
		gboolean success = status == 304 && r->catalog;
		if (fresh && json_object_is_type(fresh, json_type_array)) {
			if (r->catalog) json_object_put(r->catalog);
			r->catalog = fresh;
			fresh = NULL;
			success = TRUE;
			if (g_mkdir_with_parents(dir, 0700) != 0 || !save_json(path, r->catalog, NULL)) {
				r->warning = g_strdup("Catalog loaded, but couldn't save the cache.");
				success = FALSE;
			}
		}
		if (fresh) json_object_put(fresh);
		if (success) {
			r->checked = time(NULL);
			json_object *new_meta = json_object_new_object();
			json_object_object_add(new_meta, "checked", json_object_new_int64(r->checked));
			json_object_object_add(new_meta, "etag", json_object_new_string(etag ? etag : (meta ? text_field(meta, "etag") : "")));
			save_json(meta_path, new_meta, NULL);
			json_object_put(new_meta);
		} else if (!r->warning) {
			r->warning = g_strdup(r->catalog ? "Couldn't refresh. Using the cached catalog." : "Couldn't load the catalog. Try Refresh.");
		}
		g_free(etag);
	}
	if (meta) json_object_put(meta);
	g_free(dir);
	g_free(path);
	g_free(meta_path);
	g_task_return_pointer(task, r, free_catalog_result);
}

static void catalog_ready(GObject *source, GAsyncResult *result, gpointer user_data) {
	Installer *s = g_object_get_data(source, "installer");
	CatalogResult *r = g_task_propagate_pointer(G_TASK(result), NULL);
	if (!s->closed) {
		gtk_widget_set_sensitive(s->refresh, TRUE);
		if (r->catalog) {
			if (catalog_cache) json_object_put(catalog_cache);
			catalog_cache = json_object_get(r->catalog);
			catalog_checked = r->checked;
			if (s->catalog) json_object_put(s->catalog);
			s->catalog = json_object_get(r->catalog);
			filter_results(s);
		}
		gtk_label_set_text(GTK_LABEL(s->status), r->warning ? r->warning : "");
		if (!GPOINTER_TO_INT(g_task_get_task_data(G_TASK(result))) && r->catalog && !r->warning && time(NULL) - r->checked >= 3600)
			start_catalog(s, TRUE);
	}
	free_catalog_result(r);
}

static void start_catalog(Installer *s, gboolean refresh) {
	gtk_widget_set_sensitive(s->refresh, FALSE);
	gtk_label_set_text(GTK_LABEL(s->status), s->catalog ? "Refreshing catalog…" : "Loading catalog…");
	GTask *task = g_task_new(s->dialog, NULL, catalog_ready, NULL);
	g_task_set_task_data(task, GINT_TO_POINTER(refresh), NULL);
	g_task_run_in_thread(task, catalog_worker);
	g_object_unref(task);
}

static int count_vpks(const char *addons) {
	GDir *dir = g_dir_open(addons, 0, NULL);
	if (!dir) return 0;
	int count = 0;
	const char *name;
	while ((name = g_dir_read_name(dir))) {
		char *lower = g_ascii_strdown(name, -1);
		char *path = g_build_filename(addons, name, NULL);
		if (g_str_has_suffix(lower, ".vpk") && g_file_test(path, G_FILE_TEST_IS_REGULAR)) count++;
		g_free(lower);
		g_free(path);
	}
	g_dir_close(dir);
	return count;
}

static void read_installed(Installer *s) {
	g_hash_table_remove_all(s->installed);
	json_object *manifest = json_object_from_file(s->manifest), *mods;
	if (manifest && json_object_object_get_ex(manifest, "mods", &mods) && json_object_is_type(mods, json_type_object)) {
		json_object_object_foreach(mods, id, mod) {
			g_hash_table_add(s->installed, g_strdup(id));
		}
	}
	if (manifest) json_object_put(manifest);
	char *addons = g_path_get_dirname(s->manifest);
	s->vpk_count = count_vpks(addons);
	g_free(addons);
	char *label = g_strdup_printf("%d / %d VPKs installed", s->vpk_count, VPK_LIMIT);
	gtk_label_set_text(GTK_LABEL(s->slots), label);
	g_free(label);
}

static void selection_changed(GtkCheckButton *button, gpointer user_data) {
	Installer *s = user_data;
	int selected = 0;
	gboolean available = s->selected && !s->busy && s->vpk_count < VPK_LIMIT &&
		!g_hash_table_contains(s->installed, text_field(s->selected, "remoteId"));
	for (GtkWidget *child = gtk_widget_get_first_child(s->files); child; child = gtk_widget_get_next_sibling(child)) {
		if (!GTK_IS_CHECK_BUTTON(child)) continue;
		gtk_widget_set_sensitive(child, available);
		if (gtk_check_button_get_active(GTK_CHECK_BUTTON(child))) selected++;
	}
	gtk_widget_set_sensitive(s->install, selected > 0 && available);
}

static void free_download_task(gpointer data) {
	DownloadTask *job = data;
	g_free(job->id);
	g_free(job);
}

static void downloads_worker(GTask *task, gpointer source, gpointer task_data, GCancellable *cancel) {
	DownloadTask *job = task_data;
	json_object *obj = get_mod_downloads(job->id);
	g_task_return_pointer(task, obj, free_json);
}

static void downloads_ready(GObject *source, GAsyncResult *result, gpointer user_data) {
	Installer *s = g_object_get_data(source, "installer");
	DownloadTask *job = g_task_get_task_data(G_TASK(result));
	json_object *obj = g_task_propagate_pointer(G_TASK(result), NULL), *files;
	if (!s->closed && job->selection == s->selection) {
		gtk_box_remove(GTK_BOX(s->files), gtk_widget_get_first_child(s->files));
		if (obj && json_object_object_get_ex(obj, "downloads", &files) && json_object_is_type(files, json_type_array)) {
			s->downloads = json_object_get(files);
			int count = 0;
			GtkWidget *only = NULL;
			for (size_t i = 0; i < json_object_array_length(files); i++) {
				json_object *file = json_object_array_get_idx(files, i);
				if (get_json_int(file, "isArchived", 0) || !text_field(file, "url")[0]) continue;
				char *size = g_format_size(get_json_int(file, "size", 0));
				char *label = g_strdup_printf("%s\n%s · %s", text_field(file, "description"), text_field(file, "name"), size);
				GtkWidget *check = gtk_check_button_new();
				GtkWidget *file_label = gtk_label_new(label);
				gtk_label_set_wrap(GTK_LABEL(file_label), TRUE);
				gtk_label_set_xalign(GTK_LABEL(file_label), 0);
				gtk_check_button_set_child(GTK_CHECK_BUTTON(check), file_label);
				g_object_set_data(G_OBJECT(check), "file", file);
				gtk_widget_set_sensitive(check, s->vpk_count < VPK_LIMIT);
				gtk_box_append(GTK_BOX(s->files), check);
				g_signal_connect(check, "toggled", G_CALLBACK(selection_changed), s);
				only = check;
				count++;
				g_free(label);
				g_free(size);
			}
			if (count == 1 && s->vpk_count < VPK_LIMIT) gtk_check_button_set_active(GTK_CHECK_BUTTON(only), TRUE);
			if (!count) gtk_box_append(GTK_BOX(s->files), gtk_label_new("No downloadable files."));
		} else gtk_box_append(GTK_BOX(s->files), gtk_label_new("Couldn't load files. Select the mod again to retry."));
	}
	if (obj) json_object_put(obj);
}

static void image_worker(GTask *task, gpointer source, gpointer task_data, GCancellable *cancel) {
	g_task_return_pointer(task, get_image_texture(task_data), g_object_unref);
}

static void image_ready(GObject *source, GAsyncResult *result, gpointer user_data) {
	GdkTexture *texture = g_task_propagate_pointer(G_TASK(result), NULL);
	if (texture) {
		gtk_picture_set_paintable(GTK_PICTURE(source), GDK_PAINTABLE(texture));
		g_object_unref(texture);
	}
}

static void result_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data) {
	Installer *s = user_data;
	if (!row || s->busy) return;
	json_object *mod = g_object_get_data(G_OBJECT(row), "mod");
	if (!mod) return;
	s->selection++;
	if (s->selected) json_object_put(s->selected);
	s->selected = json_object_get(mod);
	if (s->downloads) json_object_put(s->downloads);
	s->downloads = NULL;
	gtk_widget_set_sensitive(s->install, FALSE);
	GtkWidget *child;
	while ((child = gtk_widget_get_first_child(s->details))) gtk_box_remove(GTK_BOX(s->details), child);
	GtkWidget *title = gtk_label_new(text_field(mod, "name"));
	gtk_label_set_wrap(GTK_LABEL(title), TRUE);
	gtk_widget_add_css_class(title, "title-1");
	gtk_box_append(GTK_BOX(s->details), title);
	gtk_box_append(GTK_BOX(s->details), gtk_label_new(text_field(mod, "author")));
	json_object *images;
	if (json_object_object_get_ex(mod, "images", &images) && json_object_is_type(images, json_type_array) && json_object_array_length(images)) {
		const char *url = json_object_get_string(json_object_array_get_idx(images, 0));
		if (url && url[0]) {
			GtkWidget *pic = gtk_picture_new();
			gtk_widget_set_size_request(pic, -1, 180);
			gtk_picture_set_can_shrink(GTK_PICTURE(pic), TRUE);
			gtk_box_append(GTK_BOX(s->details), pic);
			GTask *task = g_task_new(pic, NULL, image_ready, NULL);
			g_task_set_task_data(task, g_strdup(url), g_free);
			g_task_run_in_thread(task, image_worker);
			g_object_unref(task);
		}
	}
	char *desc = sanitize_html(text_field(mod, "description"));
	GtkWidget *description = gtk_label_new(desc);
	g_free(desc);
	gtk_label_set_wrap(GTK_LABEL(description), TRUE);
	gtk_label_set_xalign(GTK_LABEL(description), 0);
	gtk_box_append(GTK_BOX(s->details), description);
	s->files = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_box_append(GTK_BOX(s->details), s->files);
	if (g_hash_table_contains(s->installed, text_field(mod, "remoteId"))) {
		gtk_box_append(GTK_BOX(s->files), gtk_label_new("Already installed"));
		return;
	}
	gtk_box_append(GTK_BOX(s->files), gtk_label_new("Loading files…"));
	DownloadTask *job = g_new0(DownloadTask, 1);
	job->id = g_strdup(text_field(mod, "remoteId"));
	job->selection = s->selection;
	GTask *task = g_task_new(s->dialog, NULL, downloads_ready, NULL);
	g_task_set_task_data(task, job, free_download_task);
	g_task_run_in_thread(task, downloads_worker);
	g_object_unref(task);
}

static void show_results(Installer *s) {
	gtk_list_box_remove_all(GTK_LIST_BOX(s->results));
	int start = s->current_page * PAGE_SIZE;
	for (int i = start; i < MIN(start + PAGE_SIZE, (int)s->matches->len); i++) {
		json_object *mod = json_object_array_get_idx(s->catalog, g_array_index(s->matches, guint, i));
		GtkWidget *row = gtk_list_box_row_new();
		GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
		GtkWidget *name = gtk_label_new(text_field(mod, "name"));
		gtk_label_set_wrap(GTK_LABEL(name), TRUE);
		gtk_label_set_xalign(GTK_LABEL(name), 0);
		gtk_box_append(GTK_BOX(box), name);
		if (g_hash_table_contains(s->installed, text_field(mod, "remoteId"))) {
			GtkWidget *installed = gtk_label_new("✓ Installed");
			gtk_label_set_xalign(GTK_LABEL(installed), 0);
			gtk_widget_add_css_class(installed, "dim-label");
			gtk_box_append(GTK_BOX(box), installed);
		}
		gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
		g_object_set_data_full(G_OBJECT(row), "mod", json_object_get(mod), free_json);
		gtk_list_box_append(GTK_LIST_BOX(s->results), row);
	}
	int first = MAX(0, s->current_page - PAGE_BTN_COUNT / 2);
	first = MAX(0, MIN(first, s->total_pages - PAGE_BTN_COUNT));
	for (int i = 0; i < PAGE_BTN_COUNT; i++) {
		int page = first + i;
		char label[16];
		snprintf(label, sizeof(label), "%d", page + 1);
		gtk_button_set_label(GTK_BUTTON(s->page_btns[i]), label);
		gtk_widget_set_visible(s->page_btns[i], page < s->total_pages);
		if (page == s->current_page) gtk_widget_add_css_class(s->page_btns[i], "suggested-action");
		else gtk_widget_remove_css_class(s->page_btns[i], "suggested-action");
	}
	gtk_widget_set_sensitive(s->prev_btn, s->current_page > 0);
	gtk_widget_set_sensitive(s->next_btn, s->current_page + 1 < s->total_pages);
	if (!s->matches->len) gtk_list_box_append(GTK_LIST_BOX(s->results), gtk_label_new("No matching mods"));
}

static void filter_results(Installer *s) {
	g_array_set_size(s->matches, 0);
	char *query = g_utf8_casefold(gtk_editable_get_text(GTK_EDITABLE(s->search)), -1);
	if (s->catalog) {
		for (guint i = 0; i < json_object_array_length(s->catalog); i++) {
			json_object *mod = json_object_array_get_idx(s->catalog, i);
			char *text = g_strdup_printf("%s %s", text_field(mod, "name"), text_field(mod, "author"));
			char *folded = g_utf8_casefold(text, -1);
			if (strstr(folded, query)) g_array_append_val(s->matches, i);
			g_free(text);
			g_free(folded);
		}
	}
	g_free(query);
	s->current_page = 0;
	s->total_pages = (s->matches->len + PAGE_SIZE - 1) / PAGE_SIZE;
	show_results(s);
}

static void search_changed(GtkSearchEntry *entry, gpointer data) {
	filter_results(data);
}

static void page_clicked(GtkButton *button, gpointer data) {
	Installer *s = data;
	if (!s->total_pages) return;
	if (GTK_WIDGET(button) == s->prev_btn) s->current_page--;
	else if (GTK_WIDGET(button) == s->next_btn) s->current_page++;
	else s->current_page = atoi(gtk_button_get_label(button)) - 1;
	s->current_page = CLAMP(s->current_page, 0, s->total_pages - 1);
	show_results(s);
}

static void refresh_clicked(GtkButton *button, gpointer data) {
	Installer *s = data;
	read_installed(s);
	selection_changed(NULL, s);
	start_catalog(s, TRUE);
}

static void free_install_task(gpointer data) {
	InstallTask *job = data;
	g_free(job->manifest);
	g_free(job->id);
	json_object_put(job->files);
	g_free(job);
}

/* Flatten into our temp directory; archive paths never become filesystem paths. */
static gboolean extract_vpks(const char *path, const char *name, const char *temp, GPtrArray *vpks, GPtrArray *names, GError **error) {
	char *lower = g_ascii_strdown(name, -1);
	if (g_str_has_suffix(lower, ".vpk")) {
		g_ptr_array_add(vpks, g_strdup(path));
		g_ptr_array_add(names, g_path_get_basename(name));
		g_free(lower);
		return TRUE;
	}
	g_free(lower);
	struct archive *a = archive_read_new();
	archive_read_support_filter_all(a);
	archive_read_support_format_all(a);
	int result = archive_read_open_filename(a, path, 65536);
	struct archive_entry *entry;
	while (result == ARCHIVE_OK && (result = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
		const char *entry_name = archive_entry_pathname(entry);
		if (!entry_name) { archive_read_data_skip(a); continue; }
		lower = g_ascii_strdown(entry_name, -1);
		gboolean vpk = g_str_has_suffix(lower, ".vpk") && archive_entry_filetype(entry) == AE_IFREG;
		g_free(lower);
		if (!vpk) { archive_read_data_skip(a); continue; }
		char *filename = g_strdup_printf("vpk-%u", vpks->len);
		char *out = g_build_filename(temp, filename, NULL);
		g_free(filename);
		FILE *file = g_fopen(out, "wb");
		char buffer[65536];
		la_ssize_t length = 0;
		gboolean ok = file != NULL;
		while (ok && (length = archive_read_data(a, buffer, sizeof(buffer))) > 0)
			ok = fwrite(buffer, 1, length, file) == (size_t)length;
		if (file && fclose(file) != 0) ok = FALSE;
		if (!ok || length < 0) {
			g_unlink(out);
			g_free(out);
			result = ARCHIVE_FATAL;
			break;
		}
		g_ptr_array_add(vpks, out);
		g_ptr_array_add(names, g_path_get_basename(entry_name));
	}
	if (result != ARCHIVE_EOF) g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Couldn't extract %s: %s", name, archive_error_string(a) ? archive_error_string(a) : "couldn't write VPK");
	archive_read_free(a);
	return result == ARCHIVE_EOF;
}

static void install_worker(GTask *task, gpointer source, gpointer task_data, GCancellable *cancel) {
	InstallTask *job = task_data;
	GError *error = NULL;
	char *temp = g_dir_make_tmp("ivy-install-XXXXXX", &error);
	char *addons = g_path_get_dirname(job->manifest);
	GPtrArray *vpks = g_ptr_array_new_with_free_func(g_free);
	GPtrArray *names = g_ptr_array_new_with_free_func(g_free);
	GPtrArray *copied = g_ptr_array_new_with_free_func(g_free);
	json_object *manifest = NULL, *mods = NULL;
	if (!temp) goto done;
	for (size_t i = 0; i < json_object_array_length(job->files); i++) {
		json_object *file = json_object_array_get_idx(job->files, i);
		char *filename = g_strdup_printf("download-%zu", i);
		char *path = g_build_filename(temp, filename, NULL);
		g_free(filename);
		gboolean ok = download_file(text_field(file, "url"), path, &error) &&
			extract_vpks(path, text_field(file, "name"), temp, vpks, names, &error);
		g_free(path);
		if (!ok) goto done;
	}
	if (!vpks->len) {
		g_set_error_literal(&error, G_IO_ERROR, G_IO_ERROR_FAILED, "The selected downloads contain no VPK files.");
		goto done;
	}
	int count = count_vpks(addons);
	if (count + vpks->len > VPK_LIMIT) {
		g_set_error(&error, G_IO_ERROR, G_IO_ERROR_FAILED, "These downloads contain %u VPKs, but only %d slots are available. Select fewer files or delete some mods.", vpks->len, MAX(0, VPK_LIMIT - count));
		goto done;
	}
	/* Split VPK chunks refer to their original prefix; don't rename them independently. */
	for (guint i = 0; i < names->len; i++) {
		const char *name = g_ptr_array_index(names, i);
		size_t length = strlen(name);
		if (length >= 8 && name[length - 8] == '_' && g_ascii_isdigit(name[length - 7]) &&
			g_ascii_isdigit(name[length - 6]) && g_ascii_isdigit(name[length - 5])) {
			g_set_error_literal(&error, G_IO_ERROR, G_IO_ERROR_FAILED, "Split VPK archives aren't supported yet.");
			goto done;
		}
	}
	manifest = json_object_from_file(job->manifest);
	if (!manifest && !g_file_test(job->manifest, G_FILE_TEST_EXISTS)) {
		manifest = json_object_new_object();
		json_object_object_add(manifest, "version", json_object_new_int(1));
		json_object_object_add(manifest, "mods", json_object_new_object());
	}
	if (!manifest || !json_object_object_get_ex(manifest, "mods", &mods) || !json_object_is_type(mods, json_type_object)) {
		g_set_error_literal(&error, G_IO_ERROR, G_IO_ERROR_FAILED, "Couldn't read .dmm.json. It hasn't been changed.");
		goto done;
	}
	json_object *existing;
	if (json_object_object_get_ex(mods, job->id, &existing)) {
		g_set_error_literal(&error, G_IO_ERROR, G_IO_ERROR_FAILED, "This mod is already installed.");
		goto done;
	}
	if (g_mkdir_with_parents(addons, 0755) != 0) {
		g_set_error_literal(&error, G_IO_ERROR, G_IO_ERROR_FAILED, "Couldn't create the addons directory.");
		goto done;
	}
	json_object *mod = json_object_new_object();
	json_object *current = json_object_new_array(), *original = json_object_new_array();
	json_object_object_add(mod, "enabled", json_object_new_boolean(TRUE));
	json_object_object_add(mod, "order", NULL);
	json_object_object_add(mod, "currentVpks", current);
	json_object_object_add(mod, "disabledVpks", json_object_new_array());
	json_object_object_add(mod, "originalVpkNames", original);
	json_object_object_add(mods, job->id, mod);
	int slot = 1;
	for (guint i = 0; i < vpks->len; i++) {
		char *name = NULL, *path = NULL;
		do {
			g_free(name);
			g_free(path);
			name = g_strdup_printf("pak%02d_dir.vpk", slot++);
			path = g_build_filename(addons, name, NULL);
		} while (g_file_test(path, G_FILE_TEST_EXISTS) && slot <= VPK_LIMIT + 1);
		GFile *src = g_file_new_for_path(g_ptr_array_index(vpks, i));
		GFile *dst = g_file_new_for_path(path);
		gboolean ok = slot <= VPK_LIMIT + 1 && g_file_copy(src, dst, G_FILE_COPY_NONE, NULL, NULL, NULL, &error);
		g_object_unref(src);
		g_object_unref(dst);
		if (ok) {
			g_ptr_array_add(copied, path);
			json_object_array_add(current, json_object_new_string(name));
			json_object_array_add(original, json_object_new_string(g_ptr_array_index(names, i)));
		} else {
			g_free(path);
			if (!error) g_set_error_literal(&error, G_IO_ERROR, G_IO_ERROR_FAILED, "No unused VPK filenames are available.");
		}
		g_free(name);
		if (!ok) goto done;
	}
	save_json(job->manifest, manifest, &error);

done:
	if (error) {
		for (guint i = 0; i < copied->len; i++) g_unlink(g_ptr_array_index(copied, i));
	}
	if (temp) {
		GDir *dir = g_dir_open(temp, 0, NULL);
		const char *name;
		while (dir && (name = g_dir_read_name(dir))) {
			char *path = g_build_filename(temp, name, NULL);
			g_unlink(path);
			g_free(path);
		}
		if (dir) g_dir_close(dir);
		g_rmdir(temp);
	}
	if (manifest) json_object_put(manifest);
	g_ptr_array_unref(vpks);
	g_ptr_array_unref(names);
	g_ptr_array_unref(copied);
	g_free(temp);
	g_free(addons);
	if (error) g_task_return_error(task, error);
	else g_task_return_boolean(task, TRUE);
}

static void install_ready(GObject *source, GAsyncResult *result, gpointer user_data) {
	Installer *s = g_object_get_data(source, "installer");
	GError *error = NULL;
	gboolean ok = g_task_propagate_boolean(G_TASK(result), &error);
	s->busy = FALSE;
	adw_dialog_set_can_close(s->dialog, TRUE);
	gtk_widget_set_sensitive(s->sidebar, TRUE);
	gtk_widget_set_sensitive(s->files, TRUE);
	gtk_button_set_label(GTK_BUTTON(s->install), "Install");
	read_installed(s);
	if (ok) {
		ModInfo *info = parse_mod_json(text_field(s->selected, "remoteId"), s->selected);
		append_mod(g_object_get_data(G_OBJECT(s->window), "mod-list"), g_object_get_data(G_OBJECT(s->window), "mod-content"), info);
		gtk_label_set_text(GTK_LABEL(s->status), "Mod installed.");
		show_results(s);
		GtkWidget *child;
		while ((child = gtk_widget_get_first_child(s->files))) gtk_box_remove(GTK_BOX(s->files), child);
		gtk_box_append(GTK_BOX(s->files), gtk_label_new("Already installed"));
	} else {
		gtk_label_set_text(GTK_LABEL(s->status), error->message);
		g_error_free(error);
	}
	selection_changed(NULL, s);
}

static void install_clicked(GtkButton *button, gpointer data) {
	Installer *s = data;
	read_installed(s);
	if (s->busy || !s->selected || s->vpk_count >= VPK_LIMIT || g_hash_table_contains(s->installed, text_field(s->selected, "remoteId"))) {
		selection_changed(NULL, s);
		return;
	}
	InstallTask *job = g_new0(InstallTask, 1);
	job->manifest = g_strdup(s->manifest);
	job->id = g_strdup(text_field(s->selected, "remoteId"));
	job->files = json_object_new_array();
	for (GtkWidget *child = gtk_widget_get_first_child(s->files); child; child = gtk_widget_get_next_sibling(child)) {
		if (GTK_IS_CHECK_BUTTON(child) && gtk_check_button_get_active(GTK_CHECK_BUTTON(child)))
			json_object_array_add(job->files, json_object_get(g_object_get_data(G_OBJECT(child), "file")));
	}
	if (!json_object_array_length(job->files)) { free_install_task(job); return; }
	s->busy = TRUE;
	adw_dialog_set_can_close(s->dialog, FALSE);
	gtk_widget_set_sensitive(s->sidebar, FALSE);
	gtk_widget_set_sensitive(s->install, FALSE);
	gtk_widget_set_sensitive(s->files, FALSE);
	gtk_button_set_label(button, "Installing…");
	gtk_label_set_text(GTK_LABEL(s->status), "Downloading and extracting selected files…");
	GTask *task = g_task_new(s->dialog, NULL, install_ready, NULL);
	g_task_set_task_data(task, job, free_install_task);
	g_task_run_in_thread(task, install_worker);
	g_object_unref(task);
}

static void free_installer(gpointer data) {
	Installer *s = data;
	if (s->catalog) json_object_put(s->catalog);
	if (s->selected) json_object_put(s->selected);
	if (s->downloads) json_object_put(s->downloads);
	g_array_unref(s->matches);
	g_hash_table_unref(s->installed);
	g_free(s->manifest);
	g_object_unref(s->window);
	g_free(s);
}

static void pane_closed(AdwDialog *dialog, gpointer data) {
	Installer *s = data;
	s->closed = TRUE;
	if (install_dialog == dialog) install_dialog = NULL;
}

void mod_pane(gpointer user_data) {
	if (install_dialog) { adw_dialog_present(install_dialog, user_data); return; }
	if (!get_manifest_path()[0]) {
		request_deadlock_folder(GTK_WINDOW(user_data));
		return;
	}
	Installer *s = g_new0(Installer, 1);
	s->dialog = adw_dialog_new();
	install_dialog = s->dialog;
	s->window = g_object_ref(user_data);
	s->manifest = g_strdup(get_manifest_path());
	s->matches = g_array_new(FALSE, FALSE, sizeof(guint));
	s->installed = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	g_object_set_data_full(G_OBJECT(s->dialog), "installer", s, free_installer);
	g_signal_connect(s->dialog, "closed", G_CALLBACK(pane_closed), s);
	adw_dialog_set_title(s->dialog, "Install mods");
	adw_dialog_set_content_width(s->dialog, 800);
	adw_dialog_set_content_height(s->dialog, 800);
	GtkWidget *toolbar = adw_toolbar_view_new();
	adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), adw_header_bar_new());
	GtkWidget *layout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_set_margin_start(layout, 12);
	gtk_widget_set_margin_end(layout, 12);
	gtk_widget_set_margin_top(layout, 12);
	gtk_widget_set_margin_bottom(layout, 12);
	s->sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_size_request(s->sidebar, 240, -1);
	s->search = gtk_search_entry_new();
	gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(s->search), "Search mods…");
	g_signal_connect(s->search, "search-changed", G_CALLBACK(search_changed), s);
	gtk_box_append(GTK_BOX(s->sidebar), s->search);
	s->results = gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(s->results), GTK_SELECTION_SINGLE);
	g_signal_connect(s->results, "row-selected", G_CALLBACK(result_selected), s);
	GtkWidget *scroll = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), s->results);
	gtk_widget_set_vexpand(scroll, TRUE);
	gtk_box_append(GTK_BOX(s->sidebar), scroll);
	GtkWidget *pages = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	s->prev_btn = gtk_button_new_with_label("<");
	gtk_box_append(GTK_BOX(pages), s->prev_btn);
	g_signal_connect(s->prev_btn, "clicked", G_CALLBACK(page_clicked), s);
	for (int i = 0; i < PAGE_BTN_COUNT; i++) {
		s->page_btns[i] = gtk_button_new();
		gtk_box_append(GTK_BOX(pages), s->page_btns[i]);
		g_signal_connect(s->page_btns[i], "clicked", G_CALLBACK(page_clicked), s);
	}
	s->next_btn = gtk_button_new_with_label(">");
	gtk_box_append(GTK_BOX(pages), s->next_btn);
	g_signal_connect(s->next_btn, "clicked", G_CALLBACK(page_clicked), s);
	gtk_box_append(GTK_BOX(s->sidebar), pages);
	s->refresh = gtk_button_new_with_label("Refresh");
	g_signal_connect(s->refresh, "clicked", G_CALLBACK(refresh_clicked), s);
	gtk_box_append(GTK_BOX(s->sidebar), s->refresh);
	gtk_box_append(GTK_BOX(layout), s->sidebar);
	GtkWidget *right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_widget_set_hexpand(right, TRUE);
	s->details = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_box_append(GTK_BOX(s->details), gtk_label_new("Select a mod"));
	s->files = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_box_append(GTK_BOX(s->details), s->files);
	GtkWidget *details_scroll = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(details_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(details_scroll), s->details);
	gtk_widget_set_vexpand(details_scroll, TRUE);
	gtk_box_append(GTK_BOX(right), details_scroll);
	s->status = gtk_label_new("");
	gtk_label_set_wrap(GTK_LABEL(s->status), TRUE);
	gtk_box_append(GTK_BOX(right), s->status);
	s->slots = gtk_label_new("");
	gtk_box_append(GTK_BOX(right), s->slots);
	s->install = gtk_button_new_with_label("Install");
	gtk_widget_add_css_class(s->install, "suggested-action");
	gtk_widget_set_sensitive(s->install, FALSE);
	g_signal_connect(s->install, "clicked", G_CALLBACK(install_clicked), s);
	gtk_box_append(GTK_BOX(right), s->install);
	gtk_box_append(GTK_BOX(layout), right);
	adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), layout);
	adw_dialog_set_child(s->dialog, toolbar);
	read_installed(s);
	show_results(s);
	if (catalog_cache) {
		s->catalog = json_object_get(catalog_cache);
		filter_results(s);
		if (time(NULL) - catalog_checked >= 3600) start_catalog(s, TRUE);
	} else start_catalog(s, FALSE);
	adw_dialog_present(s->dialog, s->window);
}
