#include <gtk/gtk.h>
#include <libadwaita-1/adwaita.h>
#include <pthread.h>
#include <json-c/json.h>

#include "ui.h"
#include "net.h"

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
        GtkWidget *Row = adw_button_row_new();
        adw_preferences_row_set_title(ADW_PREFERENCES_ROW(Row), payload->info->name);

        g_object_set_data_full(G_OBJECT(Row), "mod-info", payload->info, (GDestroyNotify)free_mod_info);
        g_signal_connect(Row, "activated", G_CALLBACK(mod_selected), payload->content);

        gtk_list_box_append(GTK_LIST_BOX(payload->list_box), Row);
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
        g_print("[modlist] popen failed\n");
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

    ExecTask task = {
        .filepath = "/users/kenzie/deadlock/game/citadel/addons/.dmm.json",
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
