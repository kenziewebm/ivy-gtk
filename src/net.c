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

char *get_mod_name(const char *mod_id) {
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

    if (obj == NULL) return NULL;

    json_object *name_obj;

    if (!json_object_object_get_ex(obj, "name", &name_obj)) {
        json_object_put(obj);
        return NULL;
    }

    const char *name = json_object_get_string(name_obj);

    if (name == NULL) {
        json_object_put(obj);
        return NULL;
    }

    char *result_name = strdup(name);
    json_object_put(obj);

    return result_name;
}
