#ifndef DOMAIN_MISSION_H
#define DOMAIN_MISSION_H

#include "types.h"

int mission_create(const Mission *m);
int mission_list_open(Mission *out_arr, int *out_n);
int mission_complete(const char *username, int mission_id);

#endif /* DOMAIN_MISSION_H */
