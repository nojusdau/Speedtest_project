#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>

#define MAX_TEST_SECONDS      15.0   /* Testai negali trukti ilgiau nei 15s */
#define DEFAULT_SERVER_FILE   "speedtest_server_list.json"
#define LOCATION_API_URL      "http://ip-api.com/json/"
#define MAX_HOST_LEN          256
#define MAX_COUNTRY_LEN       128
#define MAX_CITY_LEN          128
#define MAX_PROVIDER_LEN      256
#define CANDIDATE_LIMIT       15     /* kiek serveriu tikrinam ieskant geriausio */

typedef struct {
    char     country[MAX_COUNTRY_LEN];
    char     city[MAX_CITY_LEN];
    char     provider[MAX_PROVIDER_LEN];
    char     host[MAX_HOST_LEN];   /* "host:port" formatu, kaip json faile */
    long     id;
} server_t;

typedef struct {
    server_t *items;
    size_t    count;
} server_list_t;

typedef struct {
    char   country[MAX_COUNTRY_LEN];
    char   country_code[8];
    double lat;
    double lon;
    int    valid;
} location_t;

typedef struct {
    int    ok;
    char   message[256];
} result_status_t;

#endif /* COMMON_H */
