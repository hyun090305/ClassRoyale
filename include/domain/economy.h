#ifndef DOMAIN_ECONOMY_H
#define DOMAIN_ECONOMY_H

#include "../types.h"
#include "user.h"

int econ_deposit(Bank *acc, int amount);
int econ_borrow(Bank *acc, int amount);
int econ_repay(Bank *acc, int amount);

/* Apply hourly compound interest to a user's bank.
 * - deposits: 0.1% per hour (0.001)
 * - loans:    0.15% per hour (0.0015)
 * hours must be > 0. Returns 1 on success, 0 on error.
 */
int econ_apply_hourly_interest(User *user, int hours);

#endif /* DOMAIN_ECONOMY_H */
