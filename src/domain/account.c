#include "../../include/domain/account.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "../../include/domain/user.h"
#include "../../include/core/csv.h"

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
        fprintf(acc->fp, "%ld,%d\n", time(NULL), amount);
        fflush(acc->fp);
    }
    return 1;
}

int account_recent_tx(const char *username, int limit, char *buf, size_t buflen) {
    if (!username || !buf || buflen == 0) return -1;
    char path[512];
    snprintf(path, sizeof(path), "data/txs/%s.csv", username);
    char *tmp = NULL;
    size_t tmplen = 0;
    if (!csv_read_last_lines(path, limit, &tmp, &tmplen) || !tmp) {
        buf[0] = '\0';
        return 0;
    }
    size_t tocopy = tmplen < buflen-1 ? tmplen : buflen-1;
    memcpy(buf, tmp, tocopy);
    buf[tocopy] = '\0';
    free(tmp);
    return (int)tocopy;
}
