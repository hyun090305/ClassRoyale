#include "../../include/domain/economy.h"

#include "../../include/domain/account.h"

int econ_deposit(Bank *acc, int amount) {
    if (!acc || amount <= 0) {
        return 0;
    }
    return account_adjust(acc, amount);
}

int econ_borrow(Bank *acc, int amount) {
    if (!acc || amount <= 0) {
        return 0;
    }
    return account_adjust(acc, amount);
}

int econ_repay(Bank *acc, int amount) {
    if (!acc || amount <= 0) {
        return 0;
    }
    return account_adjust(acc, -amount);
}

int econ_tax(const Bank *acc) {
    if (!acc || acc->balance <= 0) {
        return 0;
    }
    return acc->balance / 10;
}
