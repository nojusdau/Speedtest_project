#ifndef HTTP_UTILS_H
#define HTTP_UTILS_H

#include <curl/curl.h>
#include <time.h>

/* Buferis, kuriame kaupiamas gauto HTTP atsakymo turinys (naudojama
 * vietovei/JSON gauti, kur reikalingas visas atsakymas atmintyje). */
typedef struct {
    char   *data;
    size_t  size;
} mem_buffer_t;

void   mem_buffer_init(mem_buffer_t *buf);
void   mem_buffer_free(mem_buffer_t *buf);
size_t mem_write_callback(void *contents, size_t size, size_t nmemb, void *userp);

/* Duomenu isimimo (discard) funkcija, naudojama greicio testuose, kai
 * gauto turinio saugoti nereikia - svarbus tik baitu kiekis. */
size_t discard_write_callback(void *contents, size_t size, size_t nmemb, void *userp);

/* Progreso kontekstas naudojamas riboti testo trukme ir suzinoti,
 * kiek baitu buvo persiusta paskutinio uzklausimo metu. */
typedef struct {
    struct timespec start;
    double          max_seconds;
    curl_off_t      last_bytes;    /* download: dlnow, upload: ulnow */
} progress_ctx_t;

void progress_ctx_init(progress_ctx_t *ctx, double max_seconds);
int  progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                        curl_off_t ultotal, curl_off_t ulnow);

double elapsed_seconds(const struct timespec *start);

/* Atlieka paprasta HTTP GET uzklausa ir sugrazina atsakyma i mem_buffer_t.
 * timeout_sec - jungties/uzklausos laiko limitas sekundemis.
 * Grazina CURLcode reiksme (CURLE_OK jei viskas gerai). */
int http_get(const char *url, mem_buffer_t *out, long timeout_sec);

#endif /* HTTP_UTILS_H */
