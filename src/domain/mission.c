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
    /* Try to load persisted missions from data/missions.csv */
    csv_ensure_dir("data");
    char *buf = NULL;
    size_t buflen = 0;
    if (csv_read_last_lines("data/missions.csv", 10000, &buf, &buflen) && buf && buflen > 0) {
        char *p = buf;
        while (p && *p) {
            char *nl = strchr(p, '\n');
            if (nl) *nl = '\0';
            /* format: CREATE,id,name,type,reward,ts */
            if (strncmp(p, "CREATE,", 7) == 0) {
                char *s = p + 7;
                char *tok = strtok(s, ",");
                if (tok) {
                    int id = atoi(tok);
                    char *name = strtok(NULL, ",");
                    char *typ = strtok(NULL, ",");
                    char *rew = strtok(NULL, ",");
                    if (name && typ && rew && g_catalog_count < MAX_MISSIONS) {
                        Mission *slot = &g_catalog[g_catalog_count++];
                        memset(slot, 0, sizeof(*slot));
                        slot->id = id;
                        snprintf(slot->name, sizeof(slot->name), "%s", name);
                        slot->type = atoi(typ);
                        slot->reward = atoi(rew);
                        slot->completed = 0;
                        if (id >= g_next_id) g_next_id = id + 1;
                    }
                }
            }
            if (!nl) break;
            p = nl + 1;
        }
        free(buf);
    }
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
    /* persist new mission to data/missions.csv */
    csv_ensure_dir("data");
    /* sanitize name by replacing any commas with space to keep CSV simple */
    char name_safe[sizeof(slot->name)];
    snprintf(name_safe, sizeof(name_safe), "%s", slot->name);
    for (size_t i = 0; i < sizeof(name_safe); ++i) {
        if (name_safe[i] == ',') name_safe[i] = ' ';
        if (name_safe[i] == '\n' || name_safe[i] == '\r') name_safe[i] = ' ';
    }
    csv_append_row("data/missions.csv", "CREATE,%d,%s,%d,%d,%ld", slot->id, name_safe, slot->type, slot->reward, time(NULL));
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
    /* Ensure global catalog is loaded so we can populate user's mission list from it */
    ensure_seeded();
    char path[512];
    snprintf(path, sizeof(path), "data/missions/%s.csv", username);
    char *buf = NULL;
    size_t buflen = 0;
    /* Read per-user file (may contain ASSIGN and COMPLETE historically) */
    csv_read_last_lines(path, 1000, &buf, &buflen);

    /* Collect COMPLETE entries (id and ts) and ignore ASSIGN entries. */
    int completed_ids[256];
    long completed_ts[256];
    int completed_count = 0;
    if (buf && buflen > 0) {
        char *p = buf;
        while (p && *p) {
            char *nl = strchr(p, '\n');
            if (nl) *nl = '\0';
            if (strncmp(p, "COMPLETE,", 9) == 0) {
                char *s = p + 9;
                char *tok = strtok(s, ",");
                if (tok) {
                    int id = atoi(tok);
                    char *toks = strtok(NULL, ",");
                    long ts = toks ? atol(toks) : 0;
                    if (completed_count < (int)(sizeof(completed_ids)/sizeof(completed_ids[0]))) {
                        completed_ids[completed_count] = id;
                        completed_ts[completed_count] = ts;
                        completed_count++;
                    }
                }
            }
            if (!nl) break;
            p = nl + 1;
        }
        free(buf);
    }

    /* Rebuild per-user file to contain only COMPLETE lines (drop ASSIGN) */
    if (completed_count > 0) {
        /* overwrite file with only COMPLETE entries */
        FILE *f = fopen(path, "w");
        if (f) {
            for (int i = 0; i < completed_count; ++i) {
                fprintf(f, "COMPLETE,%d,%ld\n", completed_ids[i], completed_ts[i]);
            }
            fclose(f);
        }
    } else {
        /* no COMPLETE entries -> remove file if exists */
        remove(path);
    }

    /* Populate user's mission list from global catalog and mark completions */
    user->mission_count = 0;
    user->completed_missions = 0;
    user->total_missions = 0;
    for (int i = 0; i < g_catalog_count && user->mission_count < (int)(sizeof(user->missions)/sizeof(user->missions[0])); ++i) {
        Mission *g = &g_catalog[i];
        Mission *slot = &user->missions[user->mission_count++];
        *slot = *g;
        /* Determine if user completed this mission */
        int done = 0;
        for (int j = 0; j < completed_count; ++j) {
            if (completed_ids[j] == g->id) { done = 1; break; }
        }
        slot->completed = done;
        if (done) user->completed_missions += 1;
    }
    user->total_missions = user->mission_count;
    return user->mission_count;
}
