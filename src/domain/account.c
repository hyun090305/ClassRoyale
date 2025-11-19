#include "domain/account.h"

#include <stdio.h>
#include <string.h>

#include "domain/user.h"

int account_get_by_user(const char *username, Bank *out) {
    if (!username || !out) {
        return 0;
    }
    User *user = user_lookup(username);
    if (!user) {
        return 0;
    }
    *out = user->bank;
    return 1;
}

int account_adjust(Bank *acc, int amount) {
    if (!acc) {
        return 0;
    }
    long candidate = (long)acc->balance + amount;
    if (candidate < 0) {
        return 0;
    }
    acc->balance = (int)candidate;
    if (acc->fp) {
        fprintf(acc->fp, "%s %d\n", acc->name, amount);
        fflush(acc->fp);
    }
    return 1;
}
