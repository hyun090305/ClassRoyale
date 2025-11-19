#ifndef DOMAIN_ECONOMY_H
#define DOMAIN_ECONOMY_H

#include "types.h"

int econ_deposit(Bank *acc, int amount);
int econ_borrow(Bank *acc, int amount);
int econ_repay(Bank *acc, int amount);
int econ_tax(const Bank *acc);

#endif /* DOMAIN_ECONOMY_H */
