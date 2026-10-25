#include <gtk/gtk.h>
#include <libadwaita-1/adwaita.h>
#include <pthread.h>

#include "modlist.h"
#include "net.h"
#include "sanitizer.h"
#include "installer.h"

void mod_pane(gpointer user_data) {
	GtkWidget *main_window = GTK_WIDGET(user_data);
	AdwDialog *ModPane = adw_dialog_new();

	adw_dialog_set_title(ModPane, "Install mods");
	adw_dialog_set_content_width(ModPane, 600);
	adw_dialog_set_content_height(ModPane, 800);

	GtkWidget *layout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_set_margin_start(layout, 12);
	gtk_widget_set_margin_end(layout, 12);
	gtk_widget_set_margin_top(layout, 12);
	gtk_widget_set_margin_bottom(layout, 12);

	GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_size_request(sidebar, 220, -1);

	GtkWidget *search = gtk_search_entry_new();
	gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(search), "Search...");
	gtk_box_append(GTK_BOX(sidebar), search);

	GtkWidget *results = gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(results), GTK_SELECTION_SINGLE);
	gtk_widget_set_vexpand(results, TRUE);
	
	/* fill internet mods */

	GtkWidget *results_scroll = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(results_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(results_scroll), results);
	gtk_widget_set_vexpand(results_scroll, TRUE);
	gtk_box_append(GTK_BOX(sidebar), results_scroll);

	GtkWidget *pages = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_set_halign(pages, GTK_ALIGN_CENTER);

	GtkWidget *prevBtn = gtk_button_new_with_label("<");
	GtkWidget *page = gtk_label_new("1");
	GtkWidget *nextBtn = gtk_button_new_with_label(">");

	gtk_box_append(GTK_BOX(pages), prevBtn);
	gtk_box_append(GTK_BOX(pages), page);
	gtk_box_append(GTK_BOX(pages), nextBtn);
	gtk_box_append(GTK_BOX(sidebar), pages);

	GtkWidget *details_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_widget_set_hexpand(details_box, TRUE);
	gtk_widget_set_vexpand(details_box, TRUE);

	GtkWidget *details = adw_status_page_new();
	adw_status_page_set_title(ADW_STATUS_PAGE(details), "Select a mod");
	gtk_widget_set_vexpand(details, TRUE);
	gtk_box_append(GTK_BOX(details_box), details);

	GtkWidget *install = gtk_button_new_with_label("Install");
	gtk_widget_add_css_class(install, "suggested-action");
	gtk_widget_set_halign(install, GTK_ALIGN_END);

	gtk_box_append(GTK_BOX(details_box), install);

	gtk_box_append(GTK_BOX(layout), sidebar);
	gtk_box_append(GTK_BOX(layout), details_box);
	adw_dialog_set_child(ModPane, layout);

	adw_dialog_present(ModPane, main_window);
}
