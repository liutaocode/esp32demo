#pragma once
#include "idiom_pet_state.h"

/* Called before UI startup. On failure the app remains playable in RAM. */
ip_progress_t ip_storage_init(void);
/* Nonblocking latest-snapshot delivery to a permanent worker without UI refs. */
void ip_storage_save(ip_progress_t progress);
/* 0 = saved, 1 = saving, 2 = unavailable / write failed. */
unsigned ip_storage_status(void);
