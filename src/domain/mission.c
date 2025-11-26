/*
 * 파일 목적: mission 도메인 기능 구현
 * 작성자: ChatGPT
 * 작성일: 2024-06-13
 * 수정 이력: 2024-06-13 ChatGPT - 주석 규칙 적용
 */
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

/* helpers to avoid duplicates in the global catalog */
/* 함수 목적: catalog_has_id 함수는 mission 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: id
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
static int catalog_has_id(int id) {
    for (int i = 0; i < g_catalog_count; ++i) {
        if (g_catalog[i].id == id) return 1;
    }
    return 0;
}

/* 함수 목적: catalog_has_name 함수는 mission 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: name
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
static int catalog_has_name(const char *name) {
    if (!name) return 0;
    for (int i = 0; i < g_catalog_count; ++i) {
        if (strcmp(g_catalog[i].name, name) == 0) return 1;
    }
    return 0;
}

/* 함수 목적: ensure_seeded 함수는 mission 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: 없음
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
static void ensure_seeded(void) {
    if (g_seeded) {
        return;
    }
    /* existing seeding logic reads data/missions.csv into g_catalog */
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
                    /* skip if we already loaded this mission id (avoid duplicates from CSV) */
                    if (catalog_has_id(id)) {
                        /* advance to next line */
                        if (!nl) break;
                        p = nl + 1;
                        continue;
                    }
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

/* public helper: clear the seeded flag and re-run the seeder so callers
   can force the in-memory catalog to be refreshed from disk */
/* 함수 목적: mission_refresh_catalog 함수는 mission 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: 없음
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
int mission_refresh_catalog(void) {
    g_seeded = 0;
    ensure_seeded();
    return g_catalog_count;
}

/* 함수 목적: mission_create 함수는 mission 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: m
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
int mission_create(const Mission *m) {
    ensure_seeded();
    /* prevent creating duplicate missions by name */
    if (!m || g_catalog_count >= MAX_MISSIONS || catalog_has_name(m->name)) {
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
    /* append with a preceding newline if the file does not end with newline */
    FILE *fp = fopen("data/missions.csv", "a+");
    if (fp) {
        /* ensure file ends with newline before appending a new CREATE line */
        long cur = ftell(fp);
        if (cur > 0) {
            /* check last byte */
            fseek(fp, -1, SEEK_END);
            int last = fgetc(fp);
            fseek(fp, 0, SEEK_END);
            if (last != '\n') {
                fputc('\n', fp);
            }
        }
        fprintf(fp, "CREATE,%d,%s,%d,%d,%ld\n", slot->id, slot->name, slot->type, slot->reward, (long)time(NULL));
        fclose(fp);
    }
    return 1;
}

/* 함수 목적: mission_list_open 함수는 mission 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: out_arr, out_n
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
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

/* 함수 목적: mission_complete 함수는 mission 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: username, mission_id
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
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
    /* give reward to user's cash (not the balance) */
    user->bank.cash += user_mission->reward;
     /* record a transaction for the cash reward so it appears in user's txs */
     csv_ensure_dir("data");
     csv_ensure_dir("data/txs");
     char txpath[512];
     snprintf(txpath, sizeof(txpath), "data/txs/%s.csv", username);
     /* format: ts,reason,amount,balance -- balance unchanged here */
     csv_append_row(txpath, "%ld,%s,%+d,%d", (long)time(NULL), "MISSION_REWARD", user_mission->reward, user->bank.balance);
     /* persist accounts.csv so cash change is saved to disk (user_update_balance rewrites accounts.csv)
         pass current balance to trigger rewrite; this will include the updated cash field. */
     user_update_balance(username, user->bank.balance);
     /* persist completion to per-user missions CSV */
    csv_ensure_dir("data/missions");
    char path[512];
    snprintf(path, sizeof(path), "data/missions/%s.csv", username);
    csv_append_row(path, "COMPLETE,%d,%ld", mission_id, time(NULL));
    return 1;
}

/* 함수 목적: mission_load_user 함수는 mission 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: username, user
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
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
