/*
 * speedtest-c
 * ------------
 * Konsoline programa interneto rysio parsiuntimo/issiuntimo greiciui
 * ismatuoti, panasiai kaip "speedtest-cli", naudojant Speedtest Mini
 * (Ookla) tipo serveriu sarasa (speedtest_server_list.json).
 *
 * Bibliotekos: libcurl, getopt, cJSON.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <curl/curl.h>

#include "common.h"
#include "location.h"
#include "servers.h"
#include "speedtest.h"

static void print_usage(const char *prog) {
    printf(
        "Naudojimas: %s [PASIRINKTYS]\n"
        "\n"
        "Interneto greicio matavimo programa (parsisiuntimas / issiuntimas).\n"
        "\n"
        "Veiksmai (galima rinktis viena ar keleta; jei neduota nei vieno,\n"
        "rodoma si pagalba):\n"
        "  -a, --all                 Atlikti visa automatizuota testa\n"
        "                             (vietove -> geriausias serveris ->\n"
        "                              parsiuntimas -> issiuntimas)\n"
        "  -l, --location             Nustatyti tik vartotojo vietove (valstybe)\n"
        "  -f, --find-server           Rasti geriausia serveri pagal vietove\n"
        "  -d, --download              Atlikti parsisiuntimo greicio testa\n"
        "  -u, --upload                 Atlikti issiuntimo greicio testa\n"
        "\n"
        "Parametrai:\n"
        "  -s, --server HOST:PORT     Serveris, naudojamas -d / -u testams,\n"
        "                              jei nenaudojamas -f ar -a\n"
        "                              (pvz. -s speedtest.example.com:8080)\n"
        "  -c, --country VALSTYBE     Valstybes pavadinimas, naudojamas su\n"
        "                              -f, jei nenorima automatiskai nustatyti\n"
        "                              vietoves (arba norima ja perrasyti)\n"
        "  -j, --json-file KELIAS     Kelias iki serveriu saraso JSON failo\n"
        "                              (numatytasis: %s)\n"
        "  -t, --timeout SEKUNDES     Testo trukme sekundemis (maks. %.0f)\n"
        "  -h, --help                  Rodyti sia pagalba\n"
        "\n"
        "Pavyzdziai:\n"
        "  %s --all\n"
        "  %s --location\n"
        "  %s --find-server --country Lithuania\n"
        "  %s --download --server speedtest.example.com:8080\n"
        "  %s --upload --server speedtest.example.com:8080 --timeout 10\n",
        prog, DEFAULT_SERVER_FILE, MAX_TEST_SECONDS, prog, prog, prog, prog, prog);
}

int main(int argc, char **argv) {
    int want_all          = 0;
    int want_location     = 0;
    int want_find_server  = 0;
    int want_download     = 0;
    int want_upload       = 0;
    int want_help         = 0;

    char server_arg[MAX_HOST_LEN]      = {0};
    char country_arg[MAX_COUNTRY_LEN]  = {0};
    char json_file[512];
    snprintf(json_file, sizeof(json_file), "%s", DEFAULT_SERVER_FILE);
    double timeout_sec = MAX_TEST_SECONDS;

    static struct option long_options[] = {
        {"all",          no_argument,       0, 'a'},
        {"location",     no_argument,       0, 'l'},
        {"find-server",  no_argument,       0, 'f'},
        {"download",     no_argument,       0, 'd'},
        {"upload",       no_argument,       0, 'u'},
        {"server",       required_argument, 0, 's'},
        {"country",      required_argument, 0, 'c'},
        {"json-file",    required_argument, 0, 'j'},
        {"timeout",      required_argument, 0, 't'},
        {"help",         no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "alfdus:c:j:t:h", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'a': want_all = 1; break;
            case 'l': want_location = 1; break;
            case 'f': want_find_server = 1; break;
            case 'd': want_download = 1; break;
            case 'u': want_upload = 1; break;
            case 's': snprintf(server_arg, sizeof(server_arg), "%s", optarg); break;
            case 'c': snprintf(country_arg, sizeof(country_arg), "%s", optarg); break;
            case 'j': snprintf(json_file, sizeof(json_file), "%s", optarg); break;
            case 't':
                timeout_sec = atof(optarg);
                if (timeout_sec <= 0) {
                    fprintf(stderr, "[Klaida] Neteisinga --timeout reiksme: %s\n", optarg);
                    return 1;
                }
                if (timeout_sec > MAX_TEST_SECONDS) {
                    printf("[Info] --timeout apribotas iki %.0f sekundziu\n", MAX_TEST_SECONDS);
                    timeout_sec = MAX_TEST_SECONDS;
                }
                break;
            case 'h': want_help = 1; break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if (want_help || (!want_all && !want_location && !want_find_server &&
                       !want_download && !want_upload)) {
        print_usage(argv[0]);
        return want_help ? 0 : 1;
    }

    if (want_all) {
        want_location    = 1;
        want_find_server = 1;
        want_download    = 1;
        want_upload      = 1;
    }

    /* Jei bandoma rasti serveri arba atlikti pilna testa, bet nurodytas
     * konkretus serveris rankiniu budu - tai leistina, tiesiog find-server
     * rezultatas bus perrasomas rankiniu -s parametru veliau. */

    curl_global_init(CURL_GLOBAL_ALL);

    location_t   loc;
    memset(&loc, 0, sizeof(loc));
    int have_location = 0;

    server_t chosen_server;
    memset(&chosen_server, 0, sizeof(chosen_server));
    int have_server = 0;

    double download_mbps = 0.0, upload_mbps = 0.0;
    int have_download = 0, have_upload = 0;
    int exit_code = 0;

    /* --- 1. Vietoves nustatymas --- */
    if (want_location) {
        printf("== Vietoves nustatymas ==\n");
        printf("Siunciama uzklausa i IP vietoves API...\n");
        if (detect_location(&loc) == 0) {
            have_location = 1;
            printf("Nustatyta vietove: %s (%s)\n\n", loc.country, loc.country_code);
        } else {
            fprintf(stderr, "[Klaida] Nepavyko nustatyti vietoves.\n\n");
            exit_code = 1;
        }
    }

    /* --- 2. Geriausio serverio paieska --- */
    if (want_find_server) {
        printf("== Geriausio serverio paieska ==\n");

        const char *country_to_use = NULL;
        if (strlen(country_arg) > 0) {
            country_to_use = country_arg;
        } else if (have_location) {
            country_to_use = loc.country;
        } else {
            fprintf(stderr, "[Klaida] Serverio paieskai reikia valstybes - "
                             "naudokite --country arba --location.\n\n");
            exit_code = 1;
        }

        if (country_to_use) {
            server_list_t list;
            if (load_server_list(json_file, &list) == 0) {
                printf("Ikelta %zu serveriu is failo: %s\n", list.count, json_file);

                server_t *candidates = NULL;
                size_t candidate_count = filter_servers_by_country(&list, country_to_use, &candidates);

                if (candidate_count == 0) {
                    printf("Nerasta serveriu pagal valstybe \"%s\", bandoma "
                           "pasaulinis sarasas (pirmi %d serveriai).\n",
                           country_to_use, CANDIDATE_LIMIT);
                    candidates = list.items;
                    candidate_count = list.count;
                }

                printf("Tikrinami %zu kandidatai (max %d)...\n",
                       candidate_count, CANDIDATE_LIMIT);

                double latency = 0.0;
                int idx = find_lowest_latency_server(candidates, candidate_count, &latency);
                if (idx >= 0) {
                    chosen_server = candidates[idx];
                    have_server = 1;
                    printf("Pasirinktas serveris: %s | %s, %s | %s | latency %.1f ms\n\n",
                           chosen_server.host, chosen_server.city, chosen_server.country,
                           chosen_server.provider, latency);
                } else {
                    fprintf(stderr, "[Klaida] Nerastas nei vienas veikiantis serveris.\n\n");
                    exit_code = 1;
                }

                if (candidates != list.items) free(candidates);
                free_server_list(&list);
            } else {
                exit_code = 1;
            }
        }
    }

    /* --- 3. Serverio parinkimas testams (rankinis -s turi virsenybe) --- */
    const char *host_for_tests = NULL;
    if (strlen(server_arg) > 0) {
        host_for_tests = server_arg;
    } else if (have_server) {
        host_for_tests = chosen_server.host;
    }

    /* --- 4. Parsisiuntimo testas --- */
    if (want_download) {
        printf("== Parsisiuntimo greicio testas ==\n");
        if (!host_for_tests) {
            fprintf(stderr, "[Klaida] Parsisiuntimo testui reikia serverio - "
                             "naudokite --server arba --find-server.\n\n");
            exit_code = 1;
        } else {
            printf("Testuojama su serveriu: %s (trukme iki %.0f s)...\n",
                   host_for_tests, timeout_sec);
            if (run_download_test(host_for_tests, timeout_sec, &download_mbps) == 0) {
                have_download = 1;
                printf("Parsisiuntimo greitis: %.2f Mbps\n\n", download_mbps);
            } else {
                fprintf(stderr, "[Klaida] Parsisiuntimo testas nepavyko "
                                 "(serveris \"%s\" neatsako).\n\n", host_for_tests);
                exit_code = 1;
            }
        }
    }

    /* --- 5. Issiuntimo testas --- */
    if (want_upload) {
        printf("== Issiuntimo greicio testas ==\n");
        if (!host_for_tests) {
            fprintf(stderr, "[Klaida] Issiuntimo testui reikia serverio - "
                             "naudokite --server arba --find-server.\n\n");
            exit_code = 1;
        } else {
            printf("Testuojama su serveriu: %s (trukme iki %.0f s)...\n",
                   host_for_tests, timeout_sec);
            if (run_upload_test(host_for_tests, timeout_sec, &upload_mbps) == 0) {
                have_upload = 1;
                printf("Issiuntimo greitis: %.2f Mbps\n\n", upload_mbps);
            } else {
                fprintf(stderr, "[Klaida] Issiuntimo testas nepavyko "
                                 "(serveris \"%s\" neatsako).\n\n", host_for_tests);
                exit_code = 1;
            }
        }
    }

    /* --- Galutine rezultatu santrauka --- */
    printf("================= REZULTATAI =================\n");
    if (have_location) {
        printf("Vartotojo vietove:      %s\n", loc.country);
    }
    if (host_for_tests) {
        printf("Naudotas serveris:      %s", host_for_tests);
        if (have_server) {
            printf(" (%s, %s)", chosen_server.city, chosen_server.provider);
        }
        printf("\n");
    }
    if (have_download) {
        printf("Parsisiuntimo greitis:  %.2f Mbps\n", download_mbps);
    }
    if (have_upload) {
        printf("Issiuntimo greitis:     %.2f Mbps\n", upload_mbps);
    }
    printf("==============================================\n");

    curl_global_cleanup();
    return exit_code;
}
