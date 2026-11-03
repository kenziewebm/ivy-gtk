#include <gtk/gtk.h>
#include <libadwaita-1/adwaita.h>
#include <pthread.h>

#include "modlist.h"
#include "net.h"
#include "sanitizer.h"
#include "installer.h"

static void delete_response(AdwAlertDialog *dialog, const char *response, gpointer user_data) {
	if (strcmp(response, "delete") != 0) return;
	GtkWidget *row = g_object_get_data(G_OBJECT(dialog), "row");
	GtkWidget *content = g_object_get_data(G_OBJECT(dialog), "content");
	GtkWidget *details = g_object_get_data(G_OBJECT(dialog), "details");
	const char *mod_id = g_object_get_data(G_OBJECT(row), "mod-id");
	if (delete_mod(mod_id) != 0) {
		AdwDialog *error = adw_alert_dialog_new("Couldn't delete mod", "Check the terminal for details.");
		adw_alert_dialog_add_response(ADW_ALERT_DIALOG(error), "ok", "OK");
		adw_dialog_present(error, content);
		return;
	}
	if (adw_toolbar_view_get_content(ADW_TOOLBAR_VIEW(content)) == details) {
		GtkWidget *status = adw_status_page_new();
		adw_status_page_set_title(ADW_STATUS_PAGE(status), "Select a mod");
		adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(content), status);
	}
	GtkWidget *list = gtk_widget_get_parent(row);
	if (list) gtk_list_box_remove(GTK_LIST_BOX(list), row);
}

static void delete_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget *row = GTK_WIDGET(user_data);
	GtkWidget *content = g_object_get_data(G_OBJECT(button), "content");
	GtkWidget *details = g_object_get_data(G_OBJECT(button), "details");
	AdwDialog *dialog = adw_alert_dialog_new("Delete mod?", "Are you sure you want to delete this mod?");
	adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog), "cancel", "Cancel", "delete", "Delete", NULL);
	adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(dialog), "delete", ADW_RESPONSE_DESTRUCTIVE);
	adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");
	g_object_set_data_full(G_OBJECT(dialog), "row", g_object_ref(row), g_object_unref);
	g_object_set_data_full(G_OBJECT(dialog), "content", g_object_ref(content), g_object_unref);
	g_object_set_data_full(G_OBJECT(dialog), "details", g_object_ref(details), g_object_unref);
	g_signal_connect(dialog, "response", G_CALLBACK(delete_response), NULL);
	adw_dialog_present(dialog, content);
}

void mod_selected(AdwPreferencesRow *mod, gpointer user_data) {
	GtkWidget *Content = user_data;
	GtkWidget *Scrolled = gtk_scrolled_window_new();
	GtkWidget *Clamp = adw_clamp_new();
	GtkWidget *Box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);

	ModInfo *info = g_object_get_data(G_OBJECT(mod), "mod-info");

	GtkWidget *TitleLabel = gtk_label_new(info->name);
	gtk_widget_add_css_class(TitleLabel, "title-1");
	gtk_widget_set_margin_top(GTK_WIDGET(TitleLabel), 24);
	gtk_box_append(GTK_BOX(Box), TitleLabel);

	GtkWidget *AuthorLabel = gtk_label_new(info->author);
	gtk_widget_add_css_class(AuthorLabel, "dim-label");
	gtk_box_append(GTK_BOX(Box), AuthorLabel);

	if (info->image_url) {
		GtkWidget *img_box = gtk_overlay_new();
		gtk_widget_set_size_request(img_box, -1, 240);

		GtkWidget *pic = gtk_picture_new();
		gtk_picture_set_can_shrink(GTK_PICTURE(pic), TRUE);

		GtkWidget *spinner = gtk_spinner_new();
		gtk_widget_set_halign(spinner, GTK_ALIGN_CENTER);
		gtk_widget_set_valign(spinner, GTK_ALIGN_CENTER);   

		gtk_overlay_set_child(GTK_OVERLAY(img_box), pic);
		gtk_overlay_add_overlay(GTK_OVERLAY(img_box), spinner);
		gtk_box_append(GTK_BOX(Box), img_box);

		if (info->imgcache) {
			gtk_picture_set_paintable(GTK_PICTURE(pic), GDK_PAINTABLE(info->imgcache));
			gtk_widget_set_visible(spinner, FALSE);
		} else {
			gtk_spinner_start(GTK_SPINNER(spinner));
			gtk_widget_set_visible(pic, FALSE);

			AsyncImgTask *task = malloc(sizeof(AsyncImgTask));
			task->url = strdup(info->image_url);
			task->info = info;
			g_weak_ref_init(&task->pic_ref, G_OBJECT(pic));
			g_weak_ref_init(&task->spinner_ref, G_OBJECT(spinner));

			pthread_t th;
			if (pthread_create(&th, NULL, download_image_worker, task) == 0) {
				pthread_detach(th);
			}
		}
	}

	GtkWidget *Description = gtk_label_new("Masz bana masz bana!");
	char *desc = sanitize_html(info->desc);
	gtk_label_set_text(GTK_LABEL(Description), desc);
	gtk_widget_set_margin_top(GTK_WIDGET(Description), 15);
	gtk_widget_set_margin_bottom(GTK_WIDGET(Description), 15);
	gtk_label_set_wrap(GTK_LABEL(Description), TRUE);
	gtk_box_append(GTK_BOX(Box), Description);

    GtkWidget *deletemod = gtk_button_new_with_label("Delete mod");
    gtk_widget_add_css_class(deletemod, "destructive-action");
    gtk_widget_set_halign(deletemod, GTK_ALIGN_END);
    gtk_widget_set_margin_bottom(deletemod, 15);
    g_object_set_data(G_OBJECT(deletemod), "content", Content);
    g_object_set_data(G_OBJECT(deletemod), "details", Scrolled);
    g_signal_connect(deletemod, "clicked", G_CALLBACK(delete_clicked), mod);
    gtk_box_append(GTK_BOX(Box), deletemod);

	adw_clamp_set_child(ADW_CLAMP(Clamp), Box);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(Scrolled), Clamp);
	adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(Content), Scrolled);
}

void app_activate(GApplication *app, gpointer *user_data) {
	g_print("[ui] init\n");
	GtkWidget *Window;
	GtkWidget *SplitView;
	GtkWidget *Box;

	Window = adw_application_window_new(GTK_APPLICATION(app));
	gtk_window_set_title(GTK_WINDOW(Window), "Ivy");
	gtk_window_set_default_size(GTK_WINDOW(Window), 800, 600);

	/* menubar */
	GMenu *MenuBar = g_menu_new();
	GMenu *AppMenu = g_menu_new();
	g_menu_append(AppMenu, "Quit", "app.quit");

	GSimpleAction *quit_action = g_simple_action_new("quit", NULL);
	g_signal_connect_swapped(quit_action, "activate", G_CALLBACK(g_application_quit), G_APPLICATION(app));
	g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(quit_action));

#if defined(_WIN32)
	const char *quit_accels[] = {"<Alt>F4", NULL};
#else
	const char *quit_accels[] = {"<Meta>q", NULL};
#endif
	gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.quit", quit_accels);
	g_menu_append_submenu(MenuBar, "App", G_MENU_MODEL(AppMenu));
	gtk_application_set_menubar(GTK_APPLICATION(app), G_MENU_MODEL(MenuBar));

	/* sidebar */

	SplitView = adw_navigation_split_view_new();

	GtkWidget *ListBox = gtk_list_box_new();
	gtk_widget_add_css_class(GTK_WIDGET(ListBox), "navigation-sidebar");

	GtkWidget *ScrolledWindow = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrolledWindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(ScrolledWindow), ListBox);

	GtkWidget *SidebarHeader = adw_header_bar_new();
	GtkWidget *SidebarToolbar = adw_toolbar_view_new();
	adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(SidebarToolbar), SidebarHeader);
	adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(SidebarToolbar), ScrolledWindow);

	AdwNavigationPage *SidebarPage = adw_navigation_page_new(SidebarToolbar, "sidebar-toolbar");
	adw_header_bar_set_show_title(ADW_HEADER_BAR(SidebarHeader), FALSE);

	GtkWidget *InstallMods = gtk_button_new_with_label("Install mods");
	gtk_widget_add_css_class(GTK_WIDGET(InstallMods), "suggested-action");
	g_signal_connect_swapped(InstallMods, "clicked", G_CALLBACK(mod_pane), GTK_WIDGET(Window));
	gtk_list_box_append(GTK_LIST_BOX(ListBox), InstallMods);


	/* main */

	GtkWidget *ContentHeader = adw_header_bar_new();
	GtkWidget *StatusPage = adw_status_page_new();
	adw_status_page_set_title(ADW_STATUS_PAGE(StatusPage), "Select a mod");

	GtkWidget *ContentToolbar = adw_toolbar_view_new();
	g_object_set_data(G_OBJECT(Window), "mod-list", ListBox);
	g_object_set_data(G_OBJECT(Window), "mod-content", ContentToolbar);
	adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(ContentToolbar), ContentHeader);
	adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(ContentToolbar), StatusPage);

	AdwNavigationPage *ContentPage = adw_navigation_page_new(ContentToolbar, "Ivy");

	adw_navigation_split_view_set_sidebar(ADW_NAVIGATION_SPLIT_VIEW(SplitView), ADW_NAVIGATION_PAGE(SidebarPage));
	adw_navigation_split_view_set_content(ADW_NAVIGATION_SPLIT_VIEW(SplitView), ADW_NAVIGATION_PAGE(ContentPage));

	adw_application_window_set_content(ADW_APPLICATION_WINDOW(Window), SplitView);	
	gtk_window_present(GTK_WINDOW(Window));

	fill_modlist(ListBox, ContentToolbar);

	g_print("[ui] window presented\n");
}
