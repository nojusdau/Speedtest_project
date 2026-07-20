#define _GNU_SOURCE
#include "servers.h"
#include "http_utils.h"
#include <cJSON.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

static char *read_whole_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long size = ftell(f);
    if (size < 0) { fclose(f); return NULL; }
    rewind(f);

    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t read_bytes = fread(buf, 1, (size_t)size, f);
    buf[read_bytes] = '\0';
    fclose(f);
    return buf;
}

int load_server_list(const char *path, server_list_t *list) {
    list->items = NULL;
    list->count = 0;

    char *raw = read_whole_file(path);
    if (!raw) {
        fprintf(stderr, "[Klaida] Nepavyko atidaryti serveriu saraso failo: %s\n", path);
        return -1;
    }

    cJSON *root = cJSON_Parse(raw);
    free(raw);
    if (!root || !cJSON_IsArray(root)) {
        fprintf(stderr, "[Klaida] Serveriu saraso failas nera tinkamas JSON masyvas: %s\n", path);
        if (root) cJSON_Delete(root);
        return -1;
    }

    int n = cJSON_GetArraySize(root);
    if (n <= 0) {
        fprintf(stderr, "[Klaida] Serveriu sarasas tuscias: %s\n", path);
        cJSON_Delete(root);
        return -1;
    }

    server_t *items = calloc((size_t)n, sizeof(server_t));
    if (!items) {
        cJSON_Delete(root);
        return -1;
    }

    size_t count = 0;
    for (int i = 0; i < n; i++) {
        cJSON *entry = cJSON_GetArrayItem(root, i);
        if (!cJSON_IsObject(entry)) continue;

        cJSON *country  = cJSON_GetObjectItemCaseSensitive(entry, "country");
        cJSON *city     = cJSON_GetObjectItemCaseSensitive(entry, "city");
        cJSON *provider = cJSON_GetObjectItemCaseSensitive(entry, "provider");
        cJSON *host     = cJSON_GetObjectItemCaseSensitive(entry, "host");
        cJSON *id       = cJSON_GetObjectItemCaseSensitive(entry, "id");

        if (!cJSON_IsString(host) || strlen(host->valuestring) == 0) continue;

        server_t *s = &items[count];
        if (cJSON_IsString(country))  snprintf(s->country,  sizeof(s->country),  "%s", country->valuestring);
        if (cJSON_IsString(city))     snprintf(s->city,     sizeof(s->city),     "%s", city->valuestring);
        if (cJSON_IsString(provider)) snprintf(s->provider, sizeof(s->provider), "%s", provider->valuestring);
        snprintf(s->host, sizeof(s->host), "%s", host->valuestring);
        if (cJSON_IsNumber(id)) s->id = (long)id->valuedouble;

        count++;
    }

    cJSON_Delete(root);

    list->items = items;
    list->count = count;
    return 0;
}

void free_server_list(server_list_t *list) {
    free(list->items);
    list->items = NULL;
    list->count = 0;
}

size_t filter_servers_by_country(const server_list_t *list, const char *country,
                                  server_t **out_candidates) {
    *out_candidates = NULL;
    if (!list || !country || strlen(country) == 0) return 0;

    server_t *tmp = calloc(list->count, sizeof(server_t));
    if (!tmp) return 0;

    size_t found = 0;
    for (size_t i = 0; i < list->count; i++) {
        if (strcasestr(list->items[i].country, country) != NULL ||
            strcasestr(country, list->items[i].country) != NULL) {
            tmp[found++] = list->items[i];
        }
    }

    if (found == 0) {
        free(tmp);
        return 0;
    }

    server_t *result = realloc(tmp, found * sizeof(server_t));
    *out_candidates = result ? result : tmp;
    return found;
}

/* Ismatuoja atsako laika (latency) milisekundemis atliekant paprasta
 * GET uzklausa i serveri. Grazina 0 sekmes atveju. */
static int measure_latency(const char *host, double *latency_ms) {
    char url[512];
    snprintf(url, sizeof(url), "http://%s/speedtest/latency.txt", host);

    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard_write_callback);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 4L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "speedtest-c/1.0");
    
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    CURLcode res = curl_easy_perform(curl);
    clock_gettime(CLOCK_MONOTONIC, &t1);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || http_code == 0) {
        return -1;
    }

    double ms = (t1.tv_sec - t0.tv_sec) * 1000.0 +
                (t1.tv_nsec - t0.tv_nsec) / 1.0e6;
    *latency_ms = ms;
    return 0;
}

int find_lowest_latency_server(server_t *candidates, size_t count, double *out_latency_ms) {
    if (count == 0) return -1;

    size_t to_check = count < CANDIDATE_LIMIT ? count : CANDIDATE_LIMIT;
    int best_idx = -1;
    double best_latency = 1e18;

    for (size_t i = 0; i < to_check; i++) {
        double latency;
        printf("  Tikrinamas serveris: %s (%s, %s)...\n",
               candidates[i].host, candidates[i].city, candidates[i].provider);
        if (measure_latency(candidates[i].host, &latency) == 0) {
            printf("    -> atsake per %.1f ms\n", latency);
            if (latency < best_latency) {
                best_latency = latency;
                best_idx = (int)i;
            }
        } else {
            printf("    -> neatsake / neaktyvus\n");
        }
    }

    if (best_idx >= 0 && out_latency_ms) {
        *out_latency_ms = best_latency;
    }
    return best_idx;
}
