#ifndef DOMAIN_ACCOUNT_H
#define DOMAIN_ACCOUNT_H

#include "../types.h"

int account_get_by_user(const char *username, Bank *out);
int account_adjust(Bank *acc, int amount);

#endif /* DOMAIN_ACCOUNT_H */
