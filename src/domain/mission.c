#include "../../include/domain/mission.h"

#include <stdio.h>
#include <string.h>

#include "../../include/domain/account.h"
#include "../../include/domain/user.h"

static Mission g_catalog[MAX_MISSIONS];
static int g_catalog_count = 0;
static int g_next_id = 1;
static int g_seeded = 0;

static void ensure_seeded(void) {
    if (g_seeded) {
        return;
    }
    Mission daily = {0};
    daily.id = g_next_id++;
    snprintf(daily.name, sizeof(daily.name), "%s", "Attendance");
    daily.type = 1;
    daily.reward = 100;
    g_catalog[g_catalog_count++] = daily;

    Mission quiz = {0};
    quiz.id = g_next_id++;
    snprintf(quiz.name, sizeof(quiz.name), "%s", "Quiz");
    quiz.type = 2;
    quiz.reward = 200;
    g_catalog[g_catalog_count++] = quiz;

    g_seeded = 1;
}

int mission_create(const Mission *m) {
    ensure_seeded();
    if (!m || g_catalog_count >= MAX_MISSIONS) {
        return 0;
    }
    Mission *slot = &g_catalog[g_catalog_count++];
    memset(slot, 0, sizeof(*slot));
    slot->id = g_next_id++;
    snprintf(slot->name, sizeof(slot->name), "%s", m->name);
    slot->type = m->type;
    slot->reward = m->reward;
    slot->completed = 0;
    return 1;
}

int mission_list_open(Mission *out_arr, int *out_n) {
    ensure_seeded();
    if (!out_arr || !out_n) {
        return 0;
    }
    int copied = 0;
    for (int i = 0; i < g_catalog_count; ++i) {
        if (!g_catalog[i].completed) {
            out_arr[copied++] = g_catalog[i];
        }
    }
    *out_n = copied;
    return 1;
}

static Mission *find_user_mission(User *user, int mission_id) {
    if (!user) {
        return NULL;
    }
    for (int i = 0; i < user->mission_count; ++i) {
        if (user->missions[i].id == mission_id) {
            return &user->missions[i];
        }
    }
    return NULL;
}

int mission_complete(const char *username, int mission_id) {
    ensure_seeded();
    if (!username) {
        return 0;
    }
    User *user = user_lookup(username);
    if (!user) {
        return 0;
    }
    Mission *user_mission = find_user_mission(user, mission_id);
    if (!user_mission || user_mission->completed) {
        return 0;
    }
    user_mission->completed = 1;
    user->completed_missions += 1;
    if (user->total_missions < user->completed_missions) {
        user->total_missions = user->completed_missions;
    }
    account_adjust(&user->bank, user_mission->reward);
    return 1;
}
