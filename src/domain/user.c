#include "../../include/domain/user.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "../../include/domain/mission.h"
#include "../../include/core/csv.h"

static User g_users[MAX_STUDENTS];
static size_t g_user_count = 0;
static int g_seeded = 0;

static void copy_item(Item *dst, const Item *src) {
    if (!dst || !src) {
        return;
    }
    *dst = *src;
}

static void copy_mission(Mission *dst, const Mission *src) {
    if (!dst || !src) {
        return;
    }
    *dst = *src;
}

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
        rank_t role = STUDENT;
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
        if (u.bank.rating == 0) {
            u.bank.rating = 'C';
        }
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
        // flexible CSV parsing: name,balance,rating[,cash[,loan[,log]]]
        char *tokens[6] = {0};
        int tc = 0;
        char *p = strtok(line1, ",");
        while (p && tc < (int)(sizeof(tokens)/sizeof(tokens[0]))) {
            tokens[tc++] = p;
            p = strtok(NULL, ",");
        }
        if (tc < 3) continue; /* need at least name,balance,rating */
        char *name = tokens[0];
        char *balance = tokens[1];
        char *rating = tokens[2];
        char *cash_tok = tc > 3 ? tokens[3] : NULL;
        char *loan_tok = tc > 4 ? tokens[4] : NULL;
        char *log = tc > 5 ? tokens[5] : NULL;

        if (!name || !balance || !rating) {
            continue;
        }
        if (g_user_count >= MAX_STUDENTS) {
            break;
        }
        for (int i = 0; i < g_user_count; ++i) {
            if (strcmp(g_users[i].name, name) == 0) {
                g_users[i].bank.balance = atoi(balance);
                g_users[i].bank.rating  = atoi(rating);
                g_users[i].bank.cash = cash_tok ? atoi(cash_tok) : 0;
                g_users[i].bank.loan = loan_tok ? atoi(loan_tok) : 0;

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
        /* CSV format (extended): name,balance,rating,cash,loan
         * Backwards compatibility: older readers ignoring extra columns will still work.
         */
        fprintf(fp, "%s,%d,%d,%d,%d\n", g_users[i].name, g_users[i].bank.balance, (int)g_users[i].bank.rating, g_users[i].bank.cash, g_users[i].bank.loan);
    }
    fclose(fp);
    return found;
}
