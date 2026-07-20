#ifndef SPEEDTEST_H
#define SPEEDTEST_H

#include "common.h"

/* Atlieka parsisiuntimo greicio testa is nurodyto serverio ("host:port").
 * Testas trunka ne ilgiau nei max_seconds (riba MAX_TEST_SECONDS).
 * Rezultatas grazinamas megabitais per sekunde (out_mbps).
 * Grazina 0 sekmes atveju, -1 jei nepavyko prisijungti prie serverio. */
int run_download_test(const char *host, double max_seconds, double *out_mbps);

/* Atlieka issiuntimo greicio testa i nurodyta serveri ("host:port"). */
int run_upload_test(const char *host, double max_seconds, double *out_mbps);

#endif /* SPEEDTEST_H */
