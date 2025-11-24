#ifndef DOMAIN_ACCOUNT_H
#define DOMAIN_ACCOUNT_H

#include "../types.h"

int account_get_by_user(const char *username, Bank *out);
int account_adjust(Bank *acc, int amount);
/* Adjust user's account and persist transaction to data/txs/<username>.csv.
 * reason is a short string describing why the change happened (no commas/newlines).
 */
int account_add_tx(User *user, int amount, const char *reason);
/* Convenience: look up by username and add transaction. */
int account_add_tx_by_username(const char *username, int amount, const char *reason);
/* Get recent transactions for user into buf (newline separated). Returns bytes written or -1 on error. */
int account_recent_tx(const char *username, int limit, char *buf, size_t buflen);

#endif /* DOMAIN_ACCOUNT_H */
