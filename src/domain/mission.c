#include "../../include/domain/mission.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../../include/core/csv.h"
#include <stdlib.h>

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
    /* persist completion to per-user missions CSV */
    csv_ensure_dir("data/missions");
    char path[512];
    snprintf(path, sizeof(path), "data/missions/%s.csv", username);
    csv_append_row(path, "COMPLETE,%d,%ld", mission_id, time(NULL));
    return 1;
}

int mission_load_user(const char *username, User *user) {
    if (!username || !user) return -1;
    char path[512];
    snprintf(path, sizeof(path), "data/missions/%s.csv", username);
    char *buf = NULL;
    size_t buflen = 0;
    if (!csv_read_last_lines(path, 1000, &buf, &buflen) || !buf) {
        return 0; /* no file or empty */
    }
    /* parse lines in order (csv_read_last_lines returns last N lines in file order) */
    int loaded = 0;
    char *p = buf;
    while (p && *p) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = '\0';
        /* format: ASSIGN,id,name,ts  or COMPLETE,id,ts */
        if (strncmp(p, "ASSIGN,", 7) == 0) {
            char *s = p + 7;
            char *tok = strtok(s, ",");
            if (tok) {
                int id = atoi(tok);
                char *name = strtok(NULL, ",");
                if (!name) name = "(no-name)";
                /* add to user's mission list if not present */
                int found = 0;
                for (int i = 0; i < user->mission_count; ++i) {
                    if (user->missions[i].id == id) { found = 1; break; }
                }
                if (!found && user->mission_count < (int)(sizeof(user->missions)/sizeof(user->missions[0]))) {
                    Mission *slot = &user->missions[user->mission_count++];
                    memset(slot, 0, sizeof(*slot));
                    slot->id = id;
                    snprintf(slot->name, sizeof(slot->name), "%s", name);
                    slot->completed = 0;
                    loaded++;
                }
            }
        } else if (strncmp(p, "COMPLETE,", 9) == 0) {
            char *s = p + 9;
            char *tok = strtok(s, ",");
            if (tok) {
                int id = atoi(tok);
                for (int i = 0; i < user->mission_count; ++i) {
                    if (user->missions[i].id == id) {
                        if (!user->missions[i].completed) {
                            user->missions[i].completed = 1;
                            user->completed_missions += 1;
                        }
                    }
                }
            }
        }
        if (!nl) break;
        p = nl + 1;
    }
    free(buf);
    if (user->total_missions < user->mission_count) user->total_missions = user->mission_count;
    return loaded;
}
