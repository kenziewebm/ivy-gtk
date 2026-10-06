#include <gtk/gtk.h>
#include <libadwaita-1/adwaita.h>

#include "modlist.h"

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

    fill_modlist(ListBox);

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
    g_print("[ui] window presented\n");
}
