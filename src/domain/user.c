/*
 * 파일 목적: user 도메인 기능 구현
 * 작성자: ChatGPT
 * 작성일: 2024-06-13
 * 수정 이력: 2024-06-13 ChatGPT - 주석 규칙 적용
 */
#include "../../include/domain/user.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

#include "../../include/domain/mission.h"
#include "../../include/core/csv.h"

static User g_users[MAX_STUDENTS];
static size_t g_user_count = 0;
static int g_seeded = 0;

/* 함수 목적: copy_item 함수는 user 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: dst, src
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
static void copy_item(Item *dst, const Item *src) {
    if (!dst || !src) {
        return;
    }
    *dst = *src;
}

/* 함수 목적: copy_mission 함수는 user 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: dst, src
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
static void copy_mission(Mission *dst, const Mission *src) {
    if (!dst || !src) {
        return;
    }
    *dst = *src;
}

/* 함수 목적: seed_defaults 함수는 user 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: 없음
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
static void seed_defaults(void) {
    if (g_seeded) return; /* already seeded */
    g_seeded = 1;

    // 전체 유저 배열 초기화
    memset(g_users, 0, sizeof(g_users));
    g_user_count = 0;

    FILE *fp = fopen("data/users.csv", "r");
    if (!fp) {
        // 파일 못 열면 예전처럼 기본 teacher / student만 넣고 끝내기
        fprintf(stderr, "warning: could not open data.csv\n");
        return;
    }

    char line[256];

    while (fgets(line, sizeof(line), fp)) {
        // 개행 제거
        line[strcspn(line, "\r\n")] = '\0';
        // 빈 줄이면 스킵
        if (line[0] == '\0') {
            continue;
        }
        // "name,password[,role]" 파싱 (role은 optional)
        char *name = strtok(line, ",");
        char *pw   = strtok(NULL, ",");
        char *role_tok = strtok(NULL, ",");
        if (!name || !pw) {
            // 형식이 이상하면 스킵
            continue;
        }
        if (g_user_count >= MAX_STUDENTS) {
            // 더 이상 못 넣으면 중단
            break;
        }
        User u = (User){0};
        // name, id, pw
        snprintf(u.name, sizeof(u.name), "%s", name); 
        snprintf(u.pw,   sizeof(u.pw),   "%s", pw);

        // role 처리 (기본 STUDENT)
        RankEnum role = STUDENT;
        if (role_tok) {
            // 허용 형식: 숫자(1=TEACHER), 혹은 'T'/'t' 시작 또는 'teacher' 등
            if (role_tok[0] == '1' || role_tok[0] == 'T' || role_tok[0] == 't') {
                role = TEACHER;
            }
        }
        u.isadmin = role;

        // bank.name 은 이름으로
        snprintf(u.bank.name, sizeof(u.bank.name), "%s", u.name);

        // 미션 목록과 완료 상태를 per-user CSV에서 불러오기
        mission_load_user(u.name, &u);

        // 은행 기본값 설정 (users.csv에는 balance 정보가 없으므로 role 기준 초기화)
        if (u.bank.balance == 0) {
            u.bank.balance = (u.isadmin == TEACHER) ? 5000 : 1000; /* deposit */
        }
        /* ensure cash defaults to 0 */
        if (u.bank.cash == 0) {
            u.bank.cash = 0;
        }
        /* rating feature removed: no-op */
        /* ensure per-user tx file exists and attach fp for writing */
        /* holdings/items already zeroed by (User){0}; ensure counts are sensible */
        if (u.holding_count < 0) u.holding_count = 0;
        if (u.mission_count < 0) u.mission_count = 0;

        g_users[g_user_count++] = u;
    }

    fclose(fp);

    FILE *fp1 = fopen("data/accounts.csv", "r");
    if (!fp1) {
        fprintf(stderr, "warning: could not open accounts.csv\n");
        return;
    }

    char line1[256];

    while (fgets(line1, sizeof(line1), fp1)) {
        // 개행 제거
        line1[strcspn(line1, "\r\n")] = '\0';
        // 빈 줄이면 스킵
        if (line1[0] == '\0') {
            continue;
        }
        /* flexible CSV parsing (support old format with rating and new format without):
         * Old: name,balance,rating,cash,loan,last_interest_ts,log
         * New: name,balance,cash,loan,last_interest_ts,log
         */
        char *tokens[12] = {0};
        int tc = 0;
        char *p = strtok(line1, ",");
        while (p && tc < (int)(sizeof(tokens)/sizeof(tokens[0]))) {
            tokens[tc++] = p;
            p = strtok(NULL, ",");
        }
        if (tc < 2) continue; /* need at least name,balance */
        char *name = tokens[0];
        char *balance = tokens[1];
        char *cash_tok = NULL;
        char *loan_tok = NULL;
        char *last_interest_tok = NULL;
        char *log = NULL;

        /* Detect whether the file uses the old "rating" field by checking
         * whether the token that would be last_interest_ts (at index 5)
         * looks like a plausible epoch timestamp (> 1e9). If so, assume
         * old format (rating present at tokens[2]). Otherwise assume new format. */
        if (tc >= 6 && atol(tokens[5]) > 1000000000L) {
            /* Old format: name,balance,rating,cash,loan,last_interest_ts,log */
            cash_tok = tc > 3 ? tokens[3] : NULL;
            loan_tok = tc > 4 ? tokens[4] : NULL;
            last_interest_tok = tc > 5 ? tokens[5] : NULL;
            log = tc > 6 ? tokens[6] : NULL;
        } else if (tc >= 5 && atol(tokens[4]) > 1000000000L) {
            /* New format: name,balance,cash,loan,last_interest_ts,log */
            cash_tok = tc > 2 ? tokens[2] : NULL;
            loan_tok = tc > 3 ? tokens[3] : NULL;
            last_interest_tok = tc > 4 ? tokens[4] : NULL;
            log = tc > 5 ? tokens[5] : NULL;
        } else {
            /* Fallback: interpret third token as cash if present */
            cash_tok = tc > 2 ? tokens[2] : NULL;
            loan_tok = tc > 3 ? tokens[3] : NULL;
            last_interest_tok = tc > 4 ? tokens[4] : NULL;
            log = tc > 5 ? tokens[5] : NULL;
        }
        if (g_user_count >= MAX_STUDENTS) {
            break;
        }
        for (int i = 0; i < g_user_count; ++i) {
            if (strcmp(g_users[i].name, name) == 0) {
                g_users[i].bank.balance = atoi(balance);
                g_users[i].bank.cash = cash_tok ? atoi(cash_tok) : 0;
                g_users[i].bank.loan = loan_tok ? atoi(loan_tok) : 0;
                if (last_interest_tok) {
                    g_users[i].bank.last_interest_ts = atol(last_interest_tok);
                } else {
                    /* If accounts.csv lacks last_interest_ts, try to derive it from
                     * the user's transaction log (data/txs/<name>.csv) by reading
                     * the last transaction timestamp. If that fails, fall back
                     * to current time to avoid retroactive application. */
                    char txpath[512];
                    snprintf(txpath, sizeof(txpath), "data/txs/%s.csv", name);
                    char *lastbuf = NULL;
                    size_t lastlen = 0;
                    long derived_ts = 0;
                    if (csv_read_last_lines(txpath, 1, &lastbuf, &lastlen) && lastbuf) {
                        /* lastbuf contains a line like: "<ts>,...\n" */
                        char *tok = strtok(lastbuf, ",");
                        if (tok) derived_ts = atol(tok);
                        free(lastbuf);
                    }
                    if (derived_ts > 0) g_users[i].bank.last_interest_ts = derived_ts;
                    else g_users[i].bank.last_interest_ts = (long)time(NULL);
                }

                snprintf(
                    g_users[i].bank.name,
                    sizeof(g_users[i].bank.name),
                    "%s",
                    name
                );
                if (log) {
                    snprintf(g_users[i].bank.log,
                             sizeof(g_users[i].bank.log),
                             "%s", log);
                }
                break;
            }
        }
    }

    fclose(fp1);
}

/* 함수 목적: has_duplicate 함수는 user 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: username
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
static int has_duplicate(const char *username) {
    for (size_t i = 0; i < g_user_count; ++i) {
        if (strncmp(g_users[i].name, username, sizeof(g_users[i].name)) == 0) {
            return 1;
        }
    }
    return 0;
}

User *user_lookup(const char *username) {
    seed_defaults();
    if (!username) {
        return NULL;
    }
    for (size_t i = 0; i < g_user_count; ++i) {
        if (strncmp(g_users[i].name, username, sizeof(g_users[i].name)) == 0) {
            return &g_users[i];
        }
    }
    return NULL;
}

/* 함수 목적: user_count 함수는 user 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: 없음
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
size_t user_count(void) {
    seed_defaults();
    return g_user_count;
}

const User *user_at(size_t index) {
    seed_defaults();
    if (index >= g_user_count) {
        return NULL;
    }
    return &g_users[index];
}

/* 함수 목적: user_register 함수는 user 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: new_user
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
int user_register(const User *new_user) {
    seed_defaults();
    if (!new_user || g_user_count >= MAX_STUDENTS) {
        return 0;
    }
    /* reject empty name or password */
    if (!new_user->name || !new_user->pw) return 0;
    if (new_user->name[0] == '\0' || new_user->pw[0] == '\0') return 0;
    if (has_duplicate(new_user->name)) {
        return 0;
    }

    User *dst = &g_users[g_user_count++];
    memset(dst, 0, sizeof(*dst));
    snprintf(dst->name, sizeof(dst->name), "%s", new_user->name);
    snprintf(dst->id, sizeof(dst->id), "%s", new_user->id);
    snprintf(dst->pw, sizeof(dst->pw), "%s", new_user->pw);
    dst->isadmin = new_user->isadmin;
    dst->bank = new_user->bank;
    /* ensure last_interest_ts initialized */
    if (dst->bank.last_interest_ts == 0) dst->bank.last_interest_ts = (long)time(NULL);
    dst->completed_missions = new_user->completed_missions;
    dst->total_missions = new_user->total_missions;
    dst->mission_count = new_user->mission_count;
    dst->holding_count = new_user->holding_count;

    for (int i = 0; i < 10; ++i) {
        copy_item(&dst->items[i], &new_user->items[i]);
    }
    for (int i = 0; i < new_user->mission_count && i < (int)(sizeof(dst->missions) / sizeof(dst->missions[0])); ++i) {
        copy_mission(&dst->missions[i], &new_user->missions[i]);
    }
    for (int i = 0; i < new_user->holding_count && i < MAX_HOLDINGS; ++i) {
        dst->holdings[i] = new_user->holdings[i];
    }

    return 1;
}

/* 함수 목적: user_auth 함수는 user 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: username, password
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
int user_auth(const char *username, const char *password) {
    seed_defaults();
    if (!username || !password) {
        return 0;
    }
    User *user = user_lookup(username);
    if (!user) {
        return 0;
    }
    return strncmp(user->pw, password, sizeof(user->pw)) == 0;
}

// Persist updated balance to data/accounts.csv (rewrites file from in-memory user table)
/* 함수 목적: user_update_balance 함수는 user 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: username, new_balance
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
int user_update_balance(const char *username, int new_balance) {
    if (!username) return 0;
    seed_defaults(); /* ensure in-memory users are loaded */

    int found = 0;
    for (size_t i = 0; i < g_user_count; ++i) {
        if (strncmp(g_users[i].name, username, sizeof(g_users[i].name)) == 0) {
            g_users[i].bank.balance = new_balance; /* deposit */
            found = 1;
            break;
        }
    }

    /* ensure data dir exists and rewrite accounts.csv from current in-memory table */
    csv_ensure_dir("data");
    FILE *fp = fopen("data/accounts.csv", "w");
    if (!fp) {
        return found;
    }
    for (size_t i = 0; i < g_user_count; ++i) {
        /* CSV format (new): name,balance,cash,loan,last_interest_ts,log
         * For compatibility the loader accepts old rows that included a rating column.
         */
        /* Do NOT persist the in-memory bank.log into accounts.csv to avoid
         * corrupting the CSV when log contains commas/newlines. The full
         * transaction history is stored under data/txs/<username>.csv.
         */
        fprintf(fp, "%s,%d,%d,%d,%ld,%s\n",
            g_users[i].name,
            g_users[i].bank.balance,
            g_users[i].bank.cash,
            g_users[i].bank.loan,
            g_users[i].bank.last_interest_ts,
            "");
    }
    fclose(fp);
    return found;
}
