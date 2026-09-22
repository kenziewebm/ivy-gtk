#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <libadwaita-1/adwaita.h>

static void app_exit(GtkButton *btn, GtkWindow *win) {
	gtk_window_destroy(win);
}

static void app_activate(GApplication *app, gpointer *user_data) {
	g_print("[main] init\n");
	GtkWidget *Window;
	GtkWidget *SplitView;
	GtkWidget *Box;

	Window = adw_application_window_new(GTK_APPLICATION(app));
	gtk_window_set_title(GTK_WINDOW(Window), "Ivy");
	gtk_window_set_default_size(GTK_WINDOW(Window), 800, 600);

	SplitView = adw_navigation_split_view_new();

	GtkWidget *ListBox = gtk_list_box_new();
	gtk_widget_add_css_class(GTK_WIDGET(ListBox), "navigation-sidebar");

	GtkWidget *Row1 = adw_action_row_new();
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(Row1), "dummy mod 1");
	adw_action_row_set_icon_name(ADW_ACTION_ROW(Row1), "mail-read-symbolic");
	gtk_list_box_append(GTK_LIST_BOX(ListBox), Row1);

	GtkWidget *ScrolledWindow = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ScrolledWindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(ScrolledWindow), ListBox);

	GtkWidget *SidebarHeader = adw_header_bar_new();
	GtkWidget *SidebarToolbar = adw_toolbar_view_new();
	adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(SidebarToolbar), SidebarHeader);
	adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(SidebarToolbar), ScrolledWindow);

	AdwNavigationPage *SidebarPage = adw_navigation_page_new(SidebarToolbar, "sidebar-toolbar");
	adw_header_bar_set_show_title(ADW_HEADER_BAR(SidebarHeader), FALSE);

	/* main */

	GtkWidget *ContentHeader = adw_header_bar_new();
	GtkWidget *StatusPage = adw_status_page_new();
	adw_status_page_set_title(ADW_STATUS_PAGE(StatusPage), "Select an item");

	GtkWidget *ContentToolbar = adw_toolbar_view_new();
	adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(ContentToolbar), ContentHeader);
	adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(ContentToolbar), StatusPage);

	AdwNavigationPage *ContentPage = adw_navigation_page_new(ContentToolbar, "Ivy");

	adw_navigation_split_view_set_sidebar(ADW_NAVIGATION_SPLIT_VIEW(SplitView), ADW_NAVIGATION_PAGE(SidebarPage));
	adw_navigation_split_view_set_content(ADW_NAVIGATION_SPLIT_VIEW(SplitView), ADW_NAVIGATION_PAGE(ContentPage));

	adw_application_window_set_content(ADW_APPLICATION_WINDOW(Window), SplitView);	
	gtk_window_present(GTK_WINDOW(Window));
	g_print("[main] window presented\n");
}

int main(int argc, char* argv[]) {
	AdwApplication *app;
	int stat;

	app = adw_application_new("xyz.kenziewebm.ivy", G_APPLICATION_DEFAULT_FLAGS);
	g_signal_connect(app, "activate", G_CALLBACK(app_activate), NULL);
	stat = g_application_run(G_APPLICATION(app), argc, argv);
	g_object_unref(app);
	return stat;
}
