#pragma once
#include <stdbool.h>
void ra_service_start(void);
int ra_service_soc(void);
unsigned ra_service_best(void);
bool ra_service_saved(void);
void ra_service_save(unsigned score);
