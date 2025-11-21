#include "../../include/domain/user.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

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

        if (g_seeded) {
        return;
    }
    g_seeded = 1;

    // 전체 유저 배열 초기화
    memset(g_users, 0, sizeof(g_users));
    g_user_count = 0;

    FILE *fp = fopen("users.csv", "r");
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

        // "name,password" 파싱
        char *name = strtok(line, ",");
        char *pw   = strtok(NULL, ",");

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

        // role 결정: 이름이 "teacher"면 TEACHER, 아니면 STUDENT
        
        // bank.name 은 이름으로
        snprintf(u.bank.name, sizeof(u.bank.name), "%s", u.name);

        // 나머지 필드 기본값들
        u.completed_missions = 0;
        u.total_missions     = 0;
        u.mission_count      = 0;
        u.holding_count      = 0;
        // u.items / u.missions / u.holdings 는 (User){0} 때문에 이미 0 초기화됨

        g_users[g_user_count++] = u;
    }

    fclose(fp);


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
