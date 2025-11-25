#ifndef DOMAIN_QOTD_H
#define DOMAIN_QOTD_H

#include <stddef.h>

int qotd_ensure_storage(void);
int qotd_record_entry(const char *date, const char *user, const char *question, const char *status);
/* returns 1 on success, 0 on error. On success *out_users is an array of malloc'd strings, caller must free each and free the array. */
int qotd_get_solved_users_for_date(const char *date, char ***out_users, int *out_count);

#endif /* DOMAIN_QOTD_H */
