#pragma once
#include <libadwaita-1/adwaita.h>
#include "net.h"
void mod_selected(AdwPreferencesRow *mod, gpointer user_data);
void fill_modlist(GtkWidget* ListBox, GtkWidget *content);
int delete_mod(const char *mod_id);
const char *get_manifest_path(void);
void append_mod(GtkWidget *list, GtkWidget *content, ModInfo *info);
