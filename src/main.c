#include <stdarg.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <libadwaita-1/adwaita.h>

#include "ui.h"

int main(int argc, char* argv[]) {
	AdwApplication *app;
	int stat;

    g_set_application_name("Ivy Mod Manager");
	app = adw_application_new("xyz.kenziewebm.ivy", G_APPLICATION_DEFAULT_FLAGS);
	g_signal_connect(app, "activate", G_CALLBACK(app_activate), NULL);
    g_signal_connect_swapped(app, "window-removed", G_CALLBACK(g_application_quit), G_APPLICATION(app));
	stat = g_application_run(G_APPLICATION(app), argc, argv);
	g_object_unref(app);
	return stat;
}
