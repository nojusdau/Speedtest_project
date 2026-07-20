#include "speedtest.h"
#include "http_utils.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Speedtest Mini (Ookla) tipo serveriai palaiko sitos formos keliuosius
 * skirtingo dydzio "atsitiktiniu" paveiksleliu parsisiuntima. Bandome
 * keleta variantu is eiles, kol randam veikianti. */
static const char *DOWNLOAD_FILES[] = {
    "random4000x4000.jpg",
    "random3000x3000.jpg",
    "random2000x2000.jpg",
    "random1500x1500.jpg",
    "random500x500.jpg",
    "random350x350.jpg"
};
#define DOWNLOAD_FILE_COUNT (sizeof(DOWNLOAD_FILES) / sizeof(DOWNLOAD_FILES[0]))

static double clamp_duration(double requested) {
    if (requested <= 0.0 || requested > MAX_TEST_SECONDS) {
        return MAX_TEST_SECONDS;
    }
    return requested;
}

int run_download_test(const char *host, double max_seconds, double *out_mbps) {
    double duration = clamp_duration(max_seconds);

    struct timespec overall_start;
    clock_gettime(CLOCK_MONOTONIC, &overall_start);

    curl_off_t total_bytes = 0;
    int connected = 0;
    int tried_candidates = 0;
    int chosen_index = -1;

    while (elapsed_seconds(&overall_start) < duration) {
        int idx = (chosen_index >= 0) ? chosen_index : (int)(tried_candidates % DOWNLOAD_FILE_COUNT);

        char url[600];
        snprintf(url, sizeof(url), "http://%s/speedtest/%s?x=%d",
                 host, DOWNLOAD_FILES[idx], rand());

        CURL *curl = curl_easy_init();
        if (!curl) return -1;

        progress_ctx_t ctx;
        ctx.start       = overall_start;
        ctx.max_seconds = duration;
        ctx.last_bytes  = 0;

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard_write_callback);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)(duration + 5));
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "speedtest-c/1.0");

        CURLcode res = curl_easy_perform(curl);
        long code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK && code >= 200 && code < 400) {
            connected = 1;
            chosen_index = idx;
            total_bytes += ctx.last_bytes;
        } else if (res == CURLE_ABORTED_BY_CALLBACK) {
            connected = 1;
            chosen_index = idx;
            total_bytes += ctx.last_bytes;
            break; /* laikas baigesi */
        } else {
            if (chosen_index >= 0) {
                /* jau turejom veikianti url, bet dabar nepasiseke - stabdom */
                break;
            }
            tried_candidates++;
            if ((size_t)tried_candidates >= DOWNLOAD_FILE_COUNT) {
                break; /* isbandyti visi variantai, nei vienas neveikia */
            }
            continue;
        }
    }

    double total_elapsed = elapsed_seconds(&overall_start);
    if (!connected || total_bytes == 0) {
        return -1;
    }
    if (total_elapsed <= 0.0) total_elapsed = 0.001;

    *out_mbps = ((double)total_bytes * 8.0) / (total_elapsed * 1000000.0);
    return 0;
}

/* --- Issiuntimo (upload) testas --- */

#define UPLOAD_POOL_SIZE (256 * 1024)
static unsigned char upload_pool[UPLOAD_POOL_SIZE];
static int pool_initialized = 0;

static void ensure_upload_pool(void) {
    if (pool_initialized) return;
    srand((unsigned int)time(NULL));
    for (size_t i = 0; i < UPLOAD_POOL_SIZE; i++) {
        upload_pool[i] = (unsigned char)(rand() & 0xFF);
    }
    pool_initialized = 1;
}

static size_t upload_read_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    (void)userdata;
    size_t want = size * nmemb;
    if (want > UPLOAD_POOL_SIZE) want = UPLOAD_POOL_SIZE;
    memcpy(ptr, upload_pool, want);
    return want;
}

int run_upload_test(const char *host, double max_seconds, double *out_mbps) {
    double duration = clamp_duration(max_seconds);
    ensure_upload_pool();

    char url[600];
    snprintf(url, sizeof(url), "http://%s/speedtest/upload.php", host);

    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Transfer-Encoding: chunked");
    headers = curl_slist_append(headers, "Expect:");
    headers = curl_slist_append(headers, "Content-Type: application/octet-stream");

    progress_ctx_t ctx;
    progress_ctx_init(&ctx, duration);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, upload_read_callback);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard_write_callback);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)(duration + 5));
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "speedtest-c/1.0");

    CURLcode res = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    double total_elapsed = elapsed_seconds(&ctx.start);
    if (total_elapsed <= 0.0) total_elapsed = 0.001;

    int transfer_ok = (res == CURLE_OK && code >= 200 && code < 400) ||
                       (res == CURLE_ABORTED_BY_CALLBACK);

    if (!transfer_ok || ctx.last_bytes == 0) {
        return -1;
    }

    *out_mbps = ((double)ctx.last_bytes * 8.0) / (total_elapsed * 1000000.0);
    return 0;
}
