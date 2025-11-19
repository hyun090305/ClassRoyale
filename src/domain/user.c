#include "domain/user.h"

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

    User admin = {0};
    snprintf(admin.name, sizeof(admin.name), "%s", "teacher");
    snprintf(admin.id, sizeof(admin.id), "%s", "teacher");
    snprintf(admin.pw, sizeof(admin.pw), "%s", "teacher");
    admin.isadmin = TEACHER;
    snprintf(admin.bank.name, sizeof(admin.bank.name), "%s", admin.name);
    admin.bank.balance = 10000;
    admin.bank.rating = 'A';

    User student = {0};
    snprintf(student.name, sizeof(student.name), "%s", "student");
    snprintf(student.id, sizeof(student.id), "%s", "student");
    snprintf(student.pw, sizeof(student.pw), "%s", "1234");
    student.isadmin = STUDENT;
    snprintf(student.bank.name, sizeof(student.bank.name), "%s", student.name);
    student.bank.balance = 5000;
    student.bank.rating = 'B';

    g_users[g_user_count++] = admin;
    g_users[g_user_count++] = student;
    g_seeded = 1;
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
