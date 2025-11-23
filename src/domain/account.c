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
    size_t tocopy = tmplen < buflen-1 ? tmplen : buflen-1;
    memcpy(buf, tmp, tocopy);
    buf[tocopy] = '\0';
    free(tmp);
    return (int)tocopy;
}
