#pragma once
#include <json-c/json.h>
#include <gdk/gdk.h>

typedef struct {
    char *id;
    char *name;
    char *author;
    char *desc;
    char *category;
    char *hero;
    char *image_url;
    int likes;
    int downloads;
    GdkTexture *imgcache;
} ModInfo;

typedef struct {
    char *url;
    ModInfo *info;
    GWeakRef pic_ref;
    GWeakRef spinner_ref;
} AsyncImgTask;

typedef struct {
    GdkTexture *texture;
    AsyncImgTask *task;
} AsyncImgResult;

void free_mod_info(ModInfo *info);
const char *get_json_string(json_object *obj, const char *key);
signed int get_json_int(json_object *obj, const char *key, int fallback);
ModInfo *get_mod_info(const char *mod_id);
ModInfo *parse_mod_json(const char *mod_id, json_object *obj);
GdkTexture *get_image_texture(const char* url);
void *download_image_worker(void *arg);

/* JSON results belong to the caller; free new_etag with g_free. */
json_object *get_mod_catalog(const char *etag, char **new_etag, long *status);
json_object *get_mod_downloads(const char *mod_id);
gboolean download_file(const char *url, const char *path, GError **error);
