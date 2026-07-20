#ifndef SERVERS_H
#define SERVERS_H

#include "common.h"

/* Ikelia serveriu sarasa is JSON failo (formatas kaip
 * speedtest_server_list.json). Grazina 0 sekmes atveju. */
int load_server_list(const char *path, server_list_t *list);

void free_server_list(server_list_t *list);

/* Sugrupuoja i nauja sarasa (candidates) tik tuos serverius, kuriu
 * "country" laukas atitinka nurodyta pavadinima (case-insensitive,
 * dalinis sutapimas). Grazina rasta kieki. Iskvietejas turi atlaisvinti
 * grazinama masyva laisvai naudojant free(). */
size_t filter_servers_by_country(const server_list_t *list, const char *country,
                                  server_t **out_candidates);

/* Ismatuoja kiekvieno kandidato "greita" latencija (HTTP uzklausa) ir
 * grazina geriausia (maziausios latencijos) serveri. Jei nerasta - grazina
 * -1, kitu atveju indeksa candidates masyve. */
int find_lowest_latency_server(server_t *candidates, size_t count,
                                double *out_latency_ms);

#endif /* SERVERS_H */
