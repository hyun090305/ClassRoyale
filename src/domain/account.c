/*
 * 파일 목적: account 도메인 기능 구현
 * 작성자: 박시유
 */
#include "../../include/domain/account.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>

#include "../../include/domain/user.h"
#include "../../include/core/csv.h"

/* 함수 목적: user의 bank log에 msg를 추가합니다.
 * 매개변수: user, msg
 * 반환 값: 없음
 */
void bank_log_append(User *user, const char *msg) {
    if (!user || !msg) return;

    char  *log = user->bank.log;
    size_t len = strlen(log);
    size_t cap = sizeof(user->bank.log);

    if (len + 1 >= cap) {
        // 자리 없으면 포기
        return;
    }

    size_t remain = cap - len - 1; // 마지막 널 문자 고려
    snprintf(log + len, remain, "%s\n", msg);
}


/* 함수 목적: Bank 포인터 out을 통해 username에 해당하는 사용자의 계좌 정보를 가져옵니다.
 * 매개변수: username, out
 * 반환 값: 0 또는 1
 */
int account_get_by_user(const char *username, Bank *out) {
    if (!username || !out) {
        return 0;
    }
    User *user = user_lookup(username);
    if (!user) {
        return 0;
    }
    *out = user->bank; /* includes new cash field */
    return 1;
}

/* 함수 목적: account 로그에 amount 만큼 잔액을 조정합니다.
 * 매개변수: acc, amount
 * 반환 값: 0 또는 1
 */
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

/* 함수 목적: account_recent_tx 함수는 주어진 사용자의 최근 거래(transaction) 기록을 읽어 사람이
읽기 쉬운 문자열로 buf에 작성합니다
 * 매개변수: username, limit, buf, buflen
 * 반환 값: 출력된 바이트 수 (>=0), 에러 시 -1, 기록 없음 시 0
 */
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

/* 함수 목적: account_add_tx 함수는 account 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: user, amount, reason
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
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

/* 함수 목적: 사용자의 예금(balance)에서 지정한 amount만큼 인출하여
사용자의 현금(user->bank.cash)으로 옮깁니다. 내부적으로 예금 잔액을
account_adjust(&user->bank, -amount)로 감소시키고, 성공 시 현금을 증가시킨 후
data/txs/<username>.csv에 트랜잭션(타임스탬프,이유,금액,잔액)을 기록합니다
 * 매개변수: user, amount, reason
 * 반환 값: 성공: 1, 실패: 0
 */
int account_withdraw_to_cash(User *user, int amount, const char *reason) {
    if (!user || amount <= 0) return 0;
    /* withdraw from deposit (balance) to cash */
    if (!account_adjust(&user->bank, -amount)) return 0; /* reduce deposit */
    /* increase cash on-hand */
    long newcash = (long)user->bank.cash + amount;
    if (newcash > INT_MAX) return 0;
    user->bank.cash = (int)newcash;

    /* append tx noting withdrawal */
    csv_ensure_dir("data");
    csv_ensure_dir("data/txs");
    char path[512];
    snprintf(path, sizeof(path), "data/txs/%s.csv", user->name);
    /* reason will be visible and include withdrawal note */
    csv_append_row(path, "%ld,%s,%+d,%d", (long)time(NULL), reason ? reason : "WITHDRAW", -amount, user->bank.balance);
    return 1;
}

/* 함수 목적: 사용자의 현금(cash)을 은행 예금(deposit/balance)으로 옮깁니다.
   먼저 사용자의 현금이 충분한지 확인한 뒤, 현금을 차감하고 예금을 증가시킵니다.
   예금 증가가 실패하면 현금 차감은 롤백됩니다. 성공 시 `data/txs/<username>.csv`에
   트랜잭션을 기록하고(타임스탬프, 이유, 금액, 잔액), 필요한 경우 `data/accounts.csv`
   를 갱신하여 변경 내용을 디스크에 저장합니다.

   매개변수:
     - user: 대상 사용자 포인터 (NULL이면 실패)
     - amount: 이체할 금액 (>0)
     - reason: 트랜잭션 사유(선택)

   반환값:
     - 성공: 1
     - 실패: 0 (예: 인자 오류, 현금 부족, 내부 실패)
*/
int account_deposit_from_cash(User *user, int amount, const char *reason) {
    if (!user || amount <= 0) return 0;
    if (user->bank.cash < amount) return 0;
    user->bank.cash -= amount;
    if (!account_adjust(&user->bank, amount)) {
        /* rollback cash change on failure */
        user->bank.cash += amount;
        return 0;
    }
    csv_ensure_dir("data");
    csv_ensure_dir("data/txs");
    char path[512];
    snprintf(path, sizeof(path), "data/txs/%s.csv", user->name);
    csv_append_row(path, "%ld,%s,%+d,%d", (long)time(NULL), reason ? reason : "CASH_DEPOSIT", amount, user->bank.balance);
    return 1;
}

/* 함수 목적: 사용자가 대출을 신청하면 대출금(`loan`)을 늘리고 그만큼
 * 사용자의 현금(`cash`)을 지급합니다. 내부적으로 오버플로우 여부를
 * 검사한 뒤 `user->bank.loan`과 `user->bank.cash`를 증가시키고,
 * 트랜잭션 로그(`data/txs/<username>.csv`)에 대출 내역을 기록합니다.
 * 매개변수:
 *   - user: 대상 사용자 포인터 (NULL이면 실패)
 *   - amount: 대출 금액 (>0)
 *   - reason: 트랜잭션 사유(선택)
 * 반환값: 성공하면 1, 실패하면 0
 */
int account_take_loan(User *user, int amount, const char *reason) {
    if (!user || amount <= 0) return 0;
    /* increase loan and give cash to user */
    long newloan = (long)user->bank.loan + amount;
    long newcash = (long)user->bank.cash + amount;
    if (newloan > INT_MAX || newcash > INT_MAX) return 0;
    user->bank.loan = (int)newloan;
    user->bank.cash = (int)newcash;

    /* record tx */
    csv_ensure_dir("data");
    csv_ensure_dir("data/txs");
    char path[512];
    snprintf(path, sizeof(path), "data/txs/%s.csv", user->name);
    csv_append_row(path, "%ld,%s,%+d,%d", (long)time(NULL), reason ? reason : "LOAN", amount, user->bank.balance);
    return 1;
}

/* 함수 목적: 사용자가 대출을 현금으로 갚을 때 호출됩니다.
 * 동작: 사용자의 현금(cash)이 상환 금액을 충당하는지 확인하고,
 *       충분하면 `user->bank.cash`와 `user->bank.loan`을 각각 차감합니다.
 *       성공 시 트랜잭션 로그(`data/txs/<username>.csv`)에 상환 내역을 기록합니다.
 * 매개변수:
 *   - user: 대상 사용자 포인터 (NULL이면 실패)
 *   - amount: 상환할 금액 (>0)
 *   - reason: 트랜잭션 사유(선택)
 * 반환값: 성공하면 1, 실패하면 0 (예: 현금 부족, 인자 오류)
 */
int account_repay_loan(User *user, int amount, const char *reason) {
    if (!user || amount <= 0) return 0;
    /* need sufficient cash and outstanding loan */
    if (user->bank.cash < amount) return 0;
    if (user->bank.loan < amount) return 0;
    user->bank.cash -= amount;
    user->bank.loan -= amount;

    /* record tx */
    csv_ensure_dir("data");
    csv_ensure_dir("data/txs");
    char path[512];
    snprintf(path, sizeof(path), "data/txs/%s.csv", user->name);
    csv_append_row(path, "%ld,%s,%+d,%d", (long)time(NULL), reason ? reason : "LOAN_REPAY", -amount, user->bank.balance);
    return 1;
}

/* 함수 목적: 사용자에게 현금(cash)을 지급(grant)합니다.
 * 설명:
 *   - 예: 퀴즈 보상, 미션 보상, 관리자 수동 지급 등 현금이 즉시 증가해야
 *     하는 상황에서 호출합니다.
 *   - 이 함수는 메모리상의 `user->bank.cash`를 증가시키고, 변경 내역을
 *     사용자별 거래 로그 `data/txs/<username>.csv`에 한 줄로 남깁니다.
 *   - 은행 예금(`balance`)에는 영향을 주지 않습니다. `data/accounts.csv`
 *     (모든 사용자 계정 파일)을 즉시 갱신하려면 호출자 측에서
 *     `user_update_balance(user->name, user->bank.balance)`를 별도로 호출해야
 *     합니다.
 *   - 입력 검증과 오버플로우 검사를 수행하여 안전하게 동작합니다.
 *
 * 매개변수:
 *   - user: 대상 사용자 포인터 (NULL이면 실패)
 *   - amount: 지급할 금액 (양수만 허용)
 *   - reason: 트랜잭션 사유 문자열(선택, NULL 가능) — 로그에 저장될 때
 *             쉼표나 개행 문자는 공백으로 치환됩니다.
 *
 * 반환값:
 *   - 성공: 1 (현금 증가 및 트랜잭션 로그 기록 완료)
 *   - 실패: 0 (잘못된 인자, 금액 <= 0, 정수 오버플로우 등)
 */
int account_grant_cash(User *user, int amount, const char *reason) {
    if (!user || amount <= 0) return 0;
    long newcash = (long)user->bank.cash + amount;
    if (newcash > INT_MAX) return 0;
    user->bank.cash = (int)newcash;

    /* append tx noting grant (balance unchanged) */
    csv_ensure_dir("data");
    csv_ensure_dir("data/txs");
    char path[512];
    snprintf(path, sizeof(path), "data/txs/%s.csv", user->name);

    /* sanitize reason similar to account_add_tx */
    char reason_safe[128] = "";
    if (reason && reason[0]) {
        snprintf(reason_safe, sizeof(reason_safe), "%s", reason);
        for (size_t i = 0; i < sizeof(reason_safe); ++i) {
            if (reason_safe[i] == ',' || reason_safe[i] == '\n' || reason_safe[i] == '\r') reason_safe[i] = ' ';
            if (reason_safe[i] == '\0') break;
        }
    }

    csv_append_row(path, "%ld,%s,%+d,%d", (long)time(NULL), reason_safe[0] ? reason_safe : "", amount, user->bank.balance);
    return 1;
}

/* 함수 목적: 사용자 이름(username)을 받아 해당 사용자 계정에 거래를 기록합니다.
 * 설명:
 *   - 내부적으로 `user_lookup`으로 사용자 포인터를 조회한 뒤,
 *     존재하면 `account_add_tx(User *user, int amount, const char *reason)`을
 *     호출하여 실제 계좌 잔액 조정 및 거래 로그 기록을 수행합니다.
 *   - 주로 외부에서 사용자 이름만 알고 있을 때 편리하게 거래를 추가할
 *     수 있도록 제공되는 헬퍼 함수입니다.
 *   - 이 함수는 `account_add_tx`의 얄당(얄당? should be '역할' — keep Korean simple) 동작을 위임하므로
 *     실제로는 계정 잔액 조정 및 `data/txs/<username>.csv` 기록을 수행합니다.
 *
 * 매개변수:
 *   - username: 대상 사용자 이름 문자열 (NULL이거나 사용자가 없으면 실패)
 *   - amount: 조정할 금액 (양수는 입금, 음수는 출금)
 *   - reason: 트랜잭션 사유 문자열(선택)
 *
 * 반환값:
 *   - 성공: 1 (사용자 존재 및 `account_add_tx` 성공)
 *   - 실패: 0 (인자 오류, 사용자 미존재, `account_add_tx` 실패 등)
 */
int account_add_tx_by_username(const char *username, int amount, const char *reason) {
    if (!username) return 0;
    User *u = user_lookup(username);
    if (!u) return 0;
    return account_add_tx(u, amount, reason);
}
