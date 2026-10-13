#pragma once
#include <json-c/json.h>

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
} ModInfo;

void free_mod_info(ModInfo *info);
const char *get_json_string(json_object *obj, const char *key);
signed int get_json_int(json_object *obj, const char *key, int fallback);
static char *strdup_safe(const char *s);
ModInfo *get_mod_info(const char *mod_id);

