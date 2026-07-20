#include "location.h"
#include "http_utils.h"
#include <cJSON.h>
#include <stdio.h>
#include <string.h>

int detect_location(location_t *loc) {
    memset(loc, 0, sizeof(*loc));
    loc->valid = 0;

    mem_buffer_t buf;
    int rc = http_get(LOCATION_API_URL, &buf, 10L);
    if (rc != 0) {
        fprintf(stderr, "[Klaida] Nepavyko susisiekti su vietoves API "
                         "(curl kodas %d)\n", rc);
        mem_buffer_free(&buf);
        return -1;
    }

    cJSON *root = cJSON_Parse(buf.data);
    mem_buffer_free(&buf);
    if (!root) {
        fprintf(stderr, "[Klaida] Nepavyko apdoroti vietoves API atsakymo (JSON)\n");
        return -1;
    }

    cJSON *status = cJSON_GetObjectItemCaseSensitive(root, "status");
    if (!cJSON_IsString(status) || strcmp(status->valuestring, "success") != 0) {
        fprintf(stderr, "[Klaida] Vietoves API negrazino sekmingo atsakymo\n");
        cJSON_Delete(root);
        return -1;
    }

    cJSON *country      = cJSON_GetObjectItemCaseSensitive(root, "country");
    cJSON *country_code = cJSON_GetObjectItemCaseSensitive(root, "countryCode");
    cJSON *lat           = cJSON_GetObjectItemCaseSensitive(root, "lat");
    cJSON *lon           = cJSON_GetObjectItemCaseSensitive(root, "lon");

    if (cJSON_IsString(country)) {
        snprintf(loc->country, sizeof(loc->country), "%s", country->valuestring);
    }
    if (cJSON_IsString(country_code)) {
        snprintf(loc->country_code, sizeof(loc->country_code), "%s",
                  country_code->valuestring);
    }
    if (cJSON_IsNumber(lat)) loc->lat = lat->valuedouble;
    if (cJSON_IsNumber(lon)) loc->lon = lon->valuedouble;

    cJSON_Delete(root);

    if (strlen(loc->country) == 0) {
        fprintf(stderr, "[Klaida] Vietoves API atsakyme nerasta valstybe\n");
        return -1;
    }

    loc->valid = 1;
    return 0;
}
