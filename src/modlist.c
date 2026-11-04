#include <gtk/gtk.h>
#include <libadwaita-1/adwaita.h>
#include <pthread.h>
#include <json-c/json.h>
#include <errno.h>
#include <glib/gstdio.h>

#include "ui.h"
#include "net.h"
#include "steam.h"
#include "gameinfo.h"

static char *manifest_path;

const char *get_manifest_path(void) {
    return manifest_path ? manifest_path : "";
}

gboolean setup_deadlock(const char *folder, GError **error) {
    char *gameinfo = g_build_filename(folder, "game", "citadel", "gameinfo.gi", NULL);
    gboolean valid = g_file_test(gameinfo, G_FILE_TEST_IS_REGULAR);
    if (!valid) {
        g_free(gameinfo);
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
            "Please select the Deadlock folder containing game/citadel/gameinfo.gi.");
        return FALSE;
    }
    gboolean configured = check_gameinfo(gameinfo, error);
    g_free(gameinfo);
    if (!configured) return FALSE;
    char *addons = g_build_filename(folder, "game", "citadel", "addons", NULL);
    char *path = g_build_filename(addons, ".dmm.json", NULL);
    gboolean ok = TRUE;
    if (g_mkdir_with_parents(addons, 0755) != 0) {
        g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errno),
            "Couldn't create the addons directory: %s", g_strerror(errno));
        ok = FALSE;
    } else if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
        ok = g_file_set_contents(path, "{\n  \"version\": 1,\n  \"mods\": {}\n}\n", -1, error);
    }
    if (ok) {
        g_free(manifest_path);
        manifest_path = g_strdup(path);
    }
    g_free(path);
    g_free(addons);
    return ok;
}

void append_mod(GtkWidget *list, GtkWidget *content, ModInfo *info) {
    GtkWidget *row = adw_button_row_new();
    adw_preferences_row_set_use_markup(ADW_PREFERENCES_ROW(row), FALSE);
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), info->name);
    g_object_set_data_full(G_OBJECT(row), "mod-info", info, (GDestroyNotify)free_mod_info);
    g_object_set_data_full(G_OBJECT(row), "mod-id", g_strdup(info->id), g_free);
    g_signal_connect(row, "activated", G_CALLBACK(mod_selected), content);
    gtk_list_box_append(GTK_LIST_BOX(list), row);
}

typedef struct {
    const char *filepath;
    json_object *json_res;
    int status;
} ExecTask;

typedef struct {
    char *mod_id;
    GtkWidget *list_box;
    GtkWidget *content;
} ModFetchTask;

typedef struct {
    ModInfo *info;
    GtkWidget *list_box;
    GtkWidget *content;
} ModUiPayload;

static gboolean on_mod_info_ready(gpointer user_data) {
    ModUiPayload *payload = (ModUiPayload *)user_data;
    if (payload->info != NULL) {
        append_mod(payload->list_box, payload->content, payload->info);
    }
    g_free(payload);
    return G_SOURCE_REMOVE;
}

void *fetch_mod_worker(void *arg) {
    ModFetchTask *task = (ModFetchTask *)arg;
    ModInfo *info = get_mod_info(task->mod_id);
    ModUiPayload *payload = g_new0(ModUiPayload, 1);
    payload->info = info;
    payload->list_box = task->list_box;
    payload->content = task->content;

    g_idle_add(on_mod_info_ready, payload);

    free(task->mod_id);
    free(task);
    pthread_exit(NULL);
}

void *get_mods_from_file(void* arg) {
    ExecTask *task = (ExecTask*)arg;
    FILE* fd = fopen(task->filepath, "r");
    if (fd == NULL) {
        g_print("[modlist] fopen failed\n");
        task->status = -1;
        pthread_exit(NULL);
    }

    task->json_res = json_object_from_fd(fileno(fd));
    fclose(fd);
    if(task->json_res == NULL) {
        g_print("[modlist] failed to parse json\n");
        task->status = -2;
    } else {
        task->status = 0;
    }

    pthread_exit(NULL);
}

void fill_modlist(GtkWidget *ListBox, GtkWidget *content) {
    pthread_t thread;
    if (!manifest_path) {
        /* first try ~/deadlock, then resolve through Steam */
        char *folder = g_build_filename(g_get_home_dir(), "deadlock", NULL);
        GError *error = NULL;
        gboolean ready = setup_deadlock(folder, &error);
        g_free(folder);
        if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) g_clear_error(&error);
        char steam_folder[512];
        char deadlock_folder[1024];
        if (!ready && !error && get_steam_path(steam_folder, sizeof(steam_folder)) == 0 &&
            get_deadlock_path(steam_folder, deadlock_folder, sizeof(deadlock_folder)) == 0)
            ready = setup_deadlock(deadlock_folder, &error);
        if (!ready) {
            GtkWindow *window = GTK_WINDOW(gtk_widget_get_root(content));
            if (error) deadlock_setup_error(window, error->message);
            else request_deadlock_folder(window);
            g_clear_error(&error);
            return;
        }
    }

    ExecTask task = {
        .filepath = manifest_path,
        .json_res = NULL,
        .status = 0
    };

    if (pthread_create(&thread, NULL, get_mods_from_file, &task) != 0) {
        g_print("[modlist] pthread_create failed\n");
        return;
    }

    if (pthread_join(thread, NULL) != 0) {
        g_print("[modlist] pthread_join failed\n");
        return;
    }

    if (task.status == 0 && task.json_res != NULL) {
        g_print("[modlist] got mods\n");
        json_object *mods_obj = NULL;
        if (!json_object_object_get_ex(task.json_res, "mods", &mods_obj) || !json_object_is_type(mods_obj, json_type_object)) {
            g_print("[modlist] corrupted .dmm.json\n");
            return;
        }

        // todo: asyncify as well
        json_object_object_foreach(mods_obj, mod_id, mod_data) {
            ModFetchTask *task = malloc(sizeof(ModFetchTask));
            task->mod_id = strdup(mod_id);
            task->list_box = ListBox;
            task->content = content;

            pthread_t thread;
            if (pthread_create(&thread, NULL, fetch_mod_worker, task) == 0) {
                pthread_detach(thread);
            } else {
                free(task->mod_id);
                free(task);
            }
        }
        /*
           ModInfo *info = get_mod_info(mod_id); 
           GtkWidget *Row = adw_button_row_new();
           adw_preferences_row_set_title(ADW_PREFERENCES_ROW(Row), info->name);
           g_object_set_data_full(G_OBJECT(Row), "mod-info", info, (GDestroyNotify)free_mod_info);
           g_signal_connect(Row, "activated", G_CALLBACK(mod_selected), content);
           gtk_list_box_append(GTK_LIST_BOX(ListBox), Row);
           }
         */
        json_object_put(task.json_res);
} else {
    g_print("[modlist] failed to list mods: %d\n", task.status);
}
}

int delete_mod(const char *mod_id) {
    json_object *manifest = json_object_from_file(manifest_path);
    json_object *mods, *mod, *files;
    if (!manifest) return -1;
    if (!json_object_object_get_ex(manifest, "mods", &mods) ||
        !json_object_object_get_ex(mods, mod_id, &mod)) {
        json_object_put(manifest);
        return -1;
    }

    char *addons = g_path_get_dirname(manifest_path);
    const char *fields[] = {"currentVpks", "disabledVpks"};
    for (int i = 0; i < 2; i++) {
        if (!json_object_object_get_ex(mod, fields[i], &files)) continue;
        if (!json_object_is_type(files, json_type_array)) goto failed;
        for (size_t j = 0; j < json_object_array_length(files); j++) {
            json_object *item = json_object_array_get_idx(files, j);
            if (!json_object_is_type(item, json_type_string)) goto failed;
            const char *name = json_object_get_string(item);
            if (!name[0] || strchr(name, '/') || strchr(name, '\\') || !strcmp(name, ".") || !strcmp(name, "..")) goto failed;
            char *path = g_build_filename(addons, name, NULL);
            int result = remove(path);
            g_free(path);
            if (result != 0 && errno != ENOENT) goto failed;
        }
    }
    json_object_object_del(mods, mod_id);
    if (json_object_to_file_ext(manifest_path, manifest, JSON_C_TO_STRING_PRETTY) != 0) goto failed;
    g_free(addons);
    json_object_put(manifest);
    return 0;

failed:
    g_print("[modlist] failed to delete mod %s\n", mod_id);
    g_free(addons);
    json_object_put(manifest);
    return -1;
}
