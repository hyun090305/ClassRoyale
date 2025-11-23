#ifndef DOMAIN_ACCOUNT_H
#define DOMAIN_ACCOUNT_H

#include "../types.h"

int account_get_by_user(const char *username, Bank *out);
int account_adjust(Bank *acc, int amount);
/* Get recent transactions for user into buf (newline separated). Returns bytes written or -1 on error. */
int account_recent_tx(const char *username, int limit, char *buf, size_t buflen);

#endif /* DOMAIN_ACCOUNT_H */
