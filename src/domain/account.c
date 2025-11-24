#include "../../include/domain/account.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "../../include/domain/user.h"
#include "../../include/core/csv.h"

void bank_log_append(User *user, const char *msg) {
    if (!user || !msg) return;

    char  *log = user->bank.log;
    size_t len = strlen(log);
    size_t cap = sizeof(user->bank.log);

    if (len + 1 >= cap) {
        // 자리 없으면 그냥 포기 (원하면 "…(truncated)" 같은 거 붙여도 됨)
        return;
    }

    size_t remain = cap - len - 1; // 마지막 널 문자 고려
    snprintf(log + len, remain, "%s\n", msg);
}


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
        // 잔액이 음수가 되면 조정 실패
        return 0;
    }

    acc->balance = (int)candidate;

    // --- 여기부터: 메모리 로그에 기록 ---
    // "timestamp,amount,balance" 형태로 한 줄 만들기
    char entry[128];
    snprintf(entry, sizeof(entry),
             "%ld,%+d,%d",
             (long)time(NULL),  // 현재 시각 (epoch time)
             amount,            // 입금/출금 금액
             acc->balance);     // 변경 후 잔액

    // acc->log 끝에 이어붙이기
    size_t len = strlen(acc->log);
    size_t cap = sizeof(acc->log);

    if (len + 1 < cap) {  // 널 문자 자리 고려
        size_t remain = cap - len - 1;
        snprintf(acc->log + len, remain, "%s\n", entry);
    }
    // 로그가 다 찼으면 그냥 새로운 로그는 버림 (원하면 처리 추가 가능)

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
    /* tmp contains lines like: "<ts>,<amount>,<balance>\n". Parse and
       convert timestamp to readable datetime then append to `buf`. */
    size_t outpos = 0;
    char *p = tmp;
    while (p && *p && outpos + 1 < buflen) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = '\0';

        /* parse flexible formats:
         * old: ts,amount,balance
         * new: ts,reason,amount,balance
         */
        char *tok1 = strtok(p, ",");
        char *tok2 = tok1 ? strtok(NULL, ",") : NULL;
        char *tok3 = tok2 ? strtok(NULL, ",") : NULL;
        char *tok4 = tok3 ? strtok(NULL, ",") : NULL;

        long ts = tok1 ? atol(tok1) : 0;
        char reason[128] = "";
        int amount = 0;
        int balance = 0;

        if (tok4) {
            /* new format with reason */
            if (tok2) snprintf(reason, sizeof(reason), "%s", tok2);
            amount = tok3 ? atoi(tok3) : 0;
            balance = tok4 ? atoi(tok4) : 0;
        } else if (tok3) {
            /* old format without reason */
            amount = tok2 ? atoi(tok2) : 0;
            balance = tok3 ? atoi(tok3) : 0;
        } else {
            /* fallback: try to interpret second token as amount */
            amount = tok2 ? atoi(tok2) : 0;
            balance = 0;
        }

        /* format absolute datetime */
        char timestr[64];
        if (ts <= 0) {
            snprintf(timestr, sizeof(timestr), "unknown");
        } else {
            time_t tt = (time_t)ts;
            struct tm *tm = localtime(&tt);
            if (tm) strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M", tm);
            else snprintf(timestr, sizeof(timestr), "%ld", ts);
        }

        /* include reason if present */
        int wrote = 0;
        if (reason[0]) {
            wrote = snprintf(buf + outpos, buflen - outpos, "[%s] %s %+d Cr (bal %d)\n", timestr, reason, amount, balance);
        } else {
            wrote = snprintf(buf + outpos, buflen - outpos, "[%s] %+d Cr (bal %d)\n", timestr, amount, balance);
        }
        if (wrote < 0) break;
        if ((size_t)wrote >= buflen - outpos) {
            outpos = buflen - 1;
            buf[outpos] = '\0';
            break;
        }
        outpos += (size_t)wrote;

        if (!nl) break;
        p = nl + 1;
    }

    free(tmp);
    if (outpos >= buflen) outpos = buflen - 1;
    buf[outpos] = '\0';
    return (int)outpos;
}

int account_add_tx(User *user, int amount, const char *reason) {
    if (!user) return 0;

    if (!account_adjust(&user->bank, amount)) {
        return 0;
    }

    /* sanitize reason: replace commas/newlines with space */
    char reason_safe[128] = "";
    if (reason && reason[0]) {
        snprintf(reason_safe, sizeof(reason_safe), "%s", reason);
        for (size_t i = 0; i < sizeof(reason_safe); ++i) {
            if (reason_safe[i] == ',' || reason_safe[i] == '\n' || reason_safe[i] == '\r') reason_safe[i] = ' ';
            if (reason_safe[i] == '\0') break;
        }
    }

    /* ensure directory exists and append a CSV row: ts,reason,amount,balance */
    csv_ensure_dir("data");
    csv_ensure_dir("data/txs");
    char path[512];
    snprintf(path, sizeof(path), "data/txs/%s.csv", user->name);
    csv_append_row(path, "%ld,%s,%+d,%d", (long)time(NULL), reason_safe[0] ? reason_safe : "", amount, user->bank.balance);
    return 1;
}

int account_add_tx_by_username(const char *username, int amount, const char *reason) {
    if (!username) return 0;
    User *u = user_lookup(username);
    if (!u) return 0;
    return account_add_tx(u, amount, reason);
}
