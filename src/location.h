#ifndef LOCATION_H
#define LOCATION_H

#include "common.h"

/* Nustato vartotojo vietove (valstybe) pagal viesa IP adreso API
 * (ip-api.com). Grazina 0 sekmes atveju, -1 klaidos atveju. */
int detect_location(location_t *loc);

#endif /* LOCATION_H */
