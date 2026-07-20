#include "http_utils.h"
#include <stdlib.h>
#include <string.h>

void mem_buffer_init(mem_buffer_t *buf) {
    buf->data = malloc(1);
    buf->data[0] = '\0';
    buf->size = 0;
}

void mem_buffer_free(mem_buffer_t *buf) {
    free(buf->data);
    buf->data = NULL;
    buf->size = 0;
}

size_t mem_write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    mem_buffer_t *mem = (mem_buffer_t *)userp;

    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) {
        return 0; /* praneša curl'ui apie klaida */
    }
    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = '\0';

    return realsize;
}

size_t discard_write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    (void)contents;
    (void)userp;
    return size * nmemb;
}

void progress_ctx_init(progress_ctx_t *ctx, double max_seconds) {
    clock_gettime(CLOCK_MONOTONIC, &ctx->start);
    ctx->max_seconds = max_seconds;
    ctx->last_bytes  = 0;
}

double elapsed_seconds(const struct timespec *start) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    double sec  = (double)(now.tv_sec - start->tv_sec);
    double nsec = (double)(now.tv_nsec - start->tv_nsec) / 1e9;
    return sec + nsec;
}

int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                       curl_off_t ultotal, curl_off_t ulnow) {
    (void)dltotal;
    (void)ultotal;
    progress_ctx_t *ctx = (progress_ctx_t *)clientp;

    /* Naudojam didesne is dlnow/ulnow reiksme - viena is ju visada bus 0 */
    curl_off_t now_bytes = dlnow > ulnow ? dlnow : ulnow;
    ctx->last_bytes = now_bytes;

    if (elapsed_seconds(&ctx->start) >= ctx->max_seconds) {
        return 1; /* liepiam curl'ui nutraukti perdavima */
    }
    return 0;
}

int http_get(const char *url, mem_buffer_t *out, long timeout_sec) {
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    mem_buffer_init(out);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, mem_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)out);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "speedtest-c/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_sec);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeout_sec);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return (int)res;
}
