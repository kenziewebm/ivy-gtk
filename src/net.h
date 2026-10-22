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
static char *strdup_safe(const char *s);
ModInfo *get_mod_info(const char *mod_id);
static gboolean on_image_downloaded(gpointer user_data);
GdkTexture *get_image_texture(const char* url);
void *download_image_worker(void *arg);

