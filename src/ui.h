#pragma once
void app_exit(GtkButton *btn, GtkWindow *win);
void app_activate(GApplication *app, gpointer *user_data);
void request_deadlock_folder(GtkWindow *window);
void deadlock_setup_error(GtkWindow *window, const char *message);
void mod_selected(AdwPreferencesRow *mod, gpointer user_data);
