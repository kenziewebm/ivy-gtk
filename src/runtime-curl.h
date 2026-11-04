#ifndef IVY_RUNTIME_CURL_H
#define IVY_RUNTIME_CURL_H

#include <curl/curl.h>
#include <stdlib.h>

/* The launcher supplies a relocated CA bundle. libcurl does not read the
 * curl command-line tool's CURL_CA_BUNDLE environment variable itself. */
static inline CURL *ivy_curl_easy_init(void) {
    CURL *curl = curl_easy_init();
    const char *ca = getenv("IVY_CA_BUNDLE");
    if (curl && ca && *ca) {
        if (curl_easy_setopt(curl, CURLOPT_CAINFO, ca) != CURLE_OK) {
            curl_easy_cleanup(curl);
            return NULL;
        }
    }
    return curl;
}

#endif
