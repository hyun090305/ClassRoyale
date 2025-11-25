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

/* Move funds between bank (deposit) and cash on-hand. */
int account_withdraw_to_cash(User *user, int amount, const char *reason); /* deposit -> cash */
int account_deposit_from_cash(User *user, int amount, const char *reason); /* cash -> deposit */
/* Loan operations: take loan (increase loan + increase cash), repay loan (decrease loan + decrease cash) */
int account_take_loan(User *user, int amount, const char *reason);
int account_repay_loan(User *user, int amount, const char *reason);
/* Grant cash directly to user's on-hand cash (does not change deposit balance). */
int account_grant_cash(User *user, int amount, const char *reason);

#endif /* DOMAIN_ACCOUNT_H */
