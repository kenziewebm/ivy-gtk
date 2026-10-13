#include <curl/curl.h>
#include <json-c/json.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define API_MOD_GET_INFO "https://api.deadlockmods.app/api/v2/mods/%s"

struct response {
    char *data;
    size_t size;
};

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

void free_mod_info(ModInfo *info) {
    if (!info) return;
    free(info->id);
    free(info->name);
    free(info->author);
    free(info->desc);
    free(info->category);
    free(info->hero);
    free(info->image_url);
    free(info);
}

const char *get_json_string(json_object *obj, const char *key) {
    json_object *val;
    if (json_object_object_get_ex(obj, key, &val)) return json_object_get_string(val);
    return NULL;
}

signed int get_json_int(json_object *obj, const char *key, int fallback) {
    json_object *val;
    if (json_object_object_get_ex(obj, key, &val)) return json_object_get_int(val);
    return fallback;
}

static char *strdup_safe(const char *s) {
    return s ? strdup(s) : NULL;
}

size_t write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct response *r = userp;

    char *ptr = realloc(r->data, r->size + realsize + 1);
    if (!ptr) return 0;

    r->data = ptr;
    memcpy(r->data + r->size, contents, realsize);
    r->size += realsize;
    r->data[r->size] = '\0';

    return realsize;
}

ModInfo *parse_mod_json(const char *mod_id, json_object *obj) {
    if (!obj) return NULL;

    ModInfo *info = calloc(1, sizeof(ModInfo));
    info->id = strdup_safe(mod_id);
    info->name = strdup_safe(get_json_string(obj, "name"));
    info->author = strdup_safe(get_json_string(obj, "author"));
    info->desc = strdup_safe(get_json_string(obj, "description"));
    info->category = strdup_safe(get_json_string(obj, "category"));
    info->hero = strdup_safe(get_json_string(obj, "hero"));
    info->likes = get_json_int(obj, "likes", -1);
    info->downloads = get_json_int(obj, "downloadCount", -1);

    json_object *images_arr;
    if (json_object_object_get_ex(obj, "images", &images_arr) &&
        json_object_is_type(images_arr, json_type_array)) {
        if (json_object_array_length(images_arr) > 0) {
            json_object *first = json_object_array_get_idx(images_arr, 0);
            info->image_url = strdup_safe(json_object_get_string(first));
        }
    }

    return info;
}

ModInfo *get_mod_info(const char *mod_id) {
    CURL *curl = curl_easy_init();
    if (curl == NULL) return NULL;

    struct response response = {
        .data = NULL,
        .size = 0
    };

    char url[256];
    snprintf(url, sizeof(url), API_MOD_GET_INFO, mod_id);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode result = curl_easy_perform(curl);

    if (result != CURLE_OK) {
        free(response.data);
        curl_easy_cleanup(curl);
        return NULL;
    }

    json_object *obj = json_tokener_parse(response.data);

    free(response.data);
    curl_easy_cleanup(curl);

    if (!obj) return NULL;

    ModInfo *info = parse_mod_json(mod_id, obj);
    json_object_put(obj);

    return info;

}
