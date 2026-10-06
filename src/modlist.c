#include <gtk/gtk.h>
#include <libadwaita-1/adwaita.h>
#include <pthread.h>
#include <json-c/json.h>

typedef struct {
    const char *filepath;
    json_object *json_res;
    int status;
} ExecTask;

void mod_selected(AdwPreferencesRow *mod, gpointer user_data) {
    g_print("clicked %d\n", GPOINTER_TO_INT(user_data));
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

/*
void *list_mods(void *arg) {
    ExecTask *task = (ExecTask*)arg;

    FILE *fp = popen(task->command, "r");
    if (fp == NULL) {
        g_print("[modlist] popen failed\n");
        task->status = -1;
        pthread_exit(NULL);
    }

    task->json_res = json_object_from_fd(fileno(fp));
    int retcode = pclose(fp);
    if(task->json_res == NULL) {
        g_print("[modlist] failed to parse json\n");
        task->status = -2;
    } else {
        task->status = retcode;
    }

    pthread_exit(NULL);
}
*/
void fill_modlist(GtkWidget *ListBox) {
    pthread_t thread;
/*
    ExecTask task = {
        .command = "ivy-cli -a ~/deadlock/game/citadel/addons list",
        .json_res = NULL,
        .status = 0
    };
*/

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

        
        GtkWidget *Rows[20] = {0};
        char titles[20][128] = {0};
        int i = 0;
        json_object_object_foreach(mods_obj, mod_name, mod_data) {
            if (!json_object_is_type(mod_data, json_type_object)) continue;
            sprintf(titles[i], "mod id %s", mod_name);
            Rows[i] = adw_button_row_new();
            adw_preferences_row_set_title(ADW_PREFERENCES_ROW(Rows[i]), titles[i]);
            g_signal_connect(Rows[i], "activated", G_CALLBACK(mod_selected), GINT_TO_POINTER(i));
            gtk_list_box_append(GTK_LIST_BOX(ListBox), Rows[i]);
            i++;
        }
        json_object_put(task.json_res);
    } else {
        g_print("[modlist] failed to list mods: %d\n", task.status);
    }
}
