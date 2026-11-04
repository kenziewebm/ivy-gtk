#include <curl/curl.h>
#include <json-c/json.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <libadwaita-1/adwaita.h>
#include <pthread.h>
#include <glib/gstdio.h>

#include "net.h"
#include "runtime-curl.h"

#ifndef API
#define API "https://api.deadlockmods.app/api/v2/mods"
#endif

#define API_MOD_GET_INFO "https://api.deadlockmods.app/api/v2/mods/%s"

struct response {
    char *data;
    size_t size;
};

typedef struct {
	GString *body;
	char *etag;
} Response;

void free_mod_info(ModInfo *info) {
    if (!info) return;
    free(info->id);
    free(info->name);
    free(info->author);
    free(info->desc);
    free(info->category);
    free(info->hero);
    free(info->image_url);
    if (info->imgcache) g_object_unref(info->imgcache);
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
    CURL *curl = ivy_curl_easy_init();
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

GdkTexture *get_image_texture(const char *url) {
    if (!url) return NULL;
    CURL *curl = ivy_curl_easy_init();
    if (!curl) return NULL;
    struct response response = { .data = NULL, .size = 0 };

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode result = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK) {
        free(response.data);
        return NULL;
    }

    GBytes *bytes = g_bytes_new_take(response.data, response.size);
    GError *error = NULL;
    GdkTexture *texture = gdk_texture_new_from_bytes(bytes, &error);
    g_bytes_unref(bytes);

    if (!texture) {
        g_print("[net] failed to decode image: %s\n", error ? error->message : "unknown");
        if (error) g_error_free(error);
        return NULL;
    }

    return texture;
}

static gboolean on_image_downloaded(gpointer user_data) {
    AsyncImgResult *res = (AsyncImgResult *)user_data;
    AsyncImgTask *task = res->task;

    GObject *pic_obj = g_weak_ref_get(&task->pic_ref);
    GObject *spinner_obj = g_weak_ref_get(&task->spinner_ref);

    if (res->texture) {
        task->info->imgcache = g_object_ref(res->texture);
        if (pic_obj) {
            gtk_picture_set_paintable(GTK_PICTURE(pic_obj), GDK_PAINTABLE(res->texture));
            gtk_widget_set_visible(GTK_WIDGET(pic_obj), TRUE);
        }
        g_object_unref(res->texture);
    }

    if (spinner_obj) {
        gtk_spinner_stop(GTK_SPINNER(spinner_obj));
        gtk_widget_set_visible(GTK_WIDGET(spinner_obj), FALSE);
        g_object_unref(spinner_obj);
    }

    if(pic_obj) g_object_unref(pic_obj);

    g_weak_ref_clear(&task->pic_ref);
    g_weak_ref_clear(&task->spinner_ref);
    free(task->url);
    free(task);
    free(res);
    return G_SOURCE_REMOVE;
}

void *download_image_worker(void *arg) {
    AsyncImgTask *task = (AsyncImgTask *)arg;
    GdkTexture *texture = get_image_texture(task->url);
    AsyncImgResult *res = malloc(sizeof(AsyncImgResult));
    res->texture = texture;
    res->task = task;

    g_idle_add(on_image_downloaded, res);
    pthread_exit(NULL);
}

static size_t receive_body(char *data, size_t size, size_t count, void *user_data) {
	Response *r = user_data;
	g_string_append_len(r->body, data, size * count);
	return size * count;
}

static size_t receive_header(char *data, size_t size, size_t count, void *user_data) {
	Response *r = user_data;
	size_t length = size * count;
	if (length > 5 && !g_ascii_strncasecmp(data, "etag:", 5)) {
		g_free(r->etag);
		r->etag = g_strstrip(g_strndup(data + 5, length - 5));
	}
	return length;
}

static CURL *new_request(const char *url) {
	CURL *curl = ivy_curl_easy_init();
	if (!curl) return NULL;
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 20L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 600L);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	return curl;
}

static json_object *fetch_json(const char *url, const char *etag, char **new_etag, long *status) {
	CURL *curl = new_request(url);
	if (!curl) return NULL;
	Response response = {.body = g_string_new(NULL)};
	struct curl_slist *headers = NULL;
	if (etag && etag[0]) {
		char *header = g_strdup_printf("If-None-Match: %s", etag);
		headers = curl_slist_append(headers, header);
		g_free(header);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	}
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receive_body);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, receive_header);
	curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response);
	CURLcode result = curl_easy_perform(curl);
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, status);
	json_object *obj = result == CURLE_OK ? json_tokener_parse(response.body->str) : NULL;
	if (result != CURLE_OK) g_print("[net] %s: %s\n", url, curl_easy_strerror(result));
	if (new_etag) *new_etag = response.etag;
	else g_free(response.etag);
	g_string_free(response.body, TRUE);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	return obj;
}

json_object *get_mod_catalog(const char *etag, char **new_etag, long *status) {
	return fetch_json(API, etag, new_etag, status);
}

json_object *get_mod_downloads(const char *mod_id) {
	char *url = g_strdup_printf(API "/%s/downloads", mod_id);
	long status = 0;
	json_object *obj = fetch_json(url, NULL, NULL, &status);
	g_free(url);
	return obj;
}

gboolean download_file(const char *url, const char *path, GError **error) {
	CURL *curl = new_request(url);
	FILE *file = g_fopen(path, "wb");
	if (!curl || !file) {
		if (curl) curl_easy_cleanup(curl);
		if (file) fclose(file);
		g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Couldn't create the download.");
		return FALSE;
	}
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
	CURLcode result = curl_easy_perform(curl);
	int closed = fclose(file);
	curl_easy_cleanup(curl);
	if (result != CURLE_OK || closed != 0) {
		g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Download failed: %s", result != CURLE_OK ? curl_easy_strerror(result) : "couldn't save file");
		return FALSE;
	}
	return TRUE;
}
