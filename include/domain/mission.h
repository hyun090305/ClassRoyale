#ifndef DOMAIN_MISSION_H
#define DOMAIN_MISSION_H

#include "../types.h"

int mission_create(const Mission *m);
int mission_list_open(Mission *out_arr, int *out_n);
int mission_complete(const char *username, int mission_id);
/* Load persisted missions for a user from data/missions/<username>.csv
 * Returns number of missions loaded or -1 on error. */
int mission_load_user(const char *username, User *user);

#endif /* DOMAIN_MISSION_H */
