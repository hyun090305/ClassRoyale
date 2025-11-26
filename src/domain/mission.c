/*
 * 파일 목적: mission 도메인 기능 구현
 * 작성자: 박시유
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

/* 함수 목적: 주어진 미션 ID가 전역 미션 카탈로그(g_catalog)에 이미 존재하는지 검사합니다.
 * 설명:
 *   - 내부 전역 배열 `g_catalog`를 순회하여 동일한 `id`가 있는지 확인합니다.
 *   - 미션을 로드하거나 새로 생성할 때 중복 삽입을 방지하기 위해 사용됩니다.
 *
 * 매개변수:
 *   - id: 검사할 미션 ID
 *
 * 반환값:
 *   - 1: 카탈로그에 해당 ID가 존재함
 *   - 0: 존재하지 않음
 */
static int catalog_has_id(int id) {
    for (int i = 0; i < g_catalog_count; ++i) {
        if (g_catalog[i].id == id) return 1;
    }
    return 0;
}

/* 함수 목적: 주어진 미션 이름(name)이 전역 미션 카탈로그(g_catalog)에 이미 존재하는지 검사합니다.
 * 설명:
 *   - 내부 전역 배열 `g_catalog`를 순회하여 각 항목의 `name`과
 *     전달된 `name`을 `strcmp`로 비교합니다.
 *   - 미션 생성 시 이름 중복을 방지하기 위해 사용됩니다.
 *
 * 매개변수:
 *   - name: 검사할 미션 이름 (NULL이면 검사하지 않고 0 반환)
 *
 * 반환값:
 *   - 1: 카탈로그에 동일한 이름의 미션이 존재함
 *   - 0: 존재하지 않음
 */
static int catalog_has_name(const char *name) {
    if (!name) return 0;
    for (int i = 0; i < g_catalog_count; ++i) {
        if (strcmp(g_catalog[i].name, name) == 0) return 1;
    }
    return 0;
}

/* 함수 목적: 전역 미션 카탈로그(g_catalog)를 디스크(`data/missions.csv`)로부터 로드하고 초기화합니다.
 * 설명:
 *   - 프로그램 시작 또는 카탈로그가 비어 있을 때 한 번만 실행되어
 *     `g_catalog`를 채웁니다.
 *   - `data/missions.csv` 파일의 각 행은 "CREATE,id,name,type,reward,ts"
 *     형식을 따르며, 해당 정보를 파싱해 내부 카탈로그에 추가합니다.
 *   - 이미 로드된 미션(id 중복)을 방지하기 위해 `catalog_has_id`를 사용합니다.
 *   - 최대 `MAX_MISSIONS`까지 로드하며, 로드 중 `g_next_id`를 갱신합니다.
 *   - 함수는 재진입 안전성(re-entrant)을 고려하여 `g_seeded`가 설정되어
 *     있으면 즉시 반환합니다.
 *
 * 매개변수:
 *   - 없음
 *
 * 반환값:
 *   - 없음 (void). 내부 전역 변수(`g_catalog`, `g_catalog_count`, `g_next_id`, `g_seeded`)를 변경합니다.
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
        buf = NULL;
    }
    g_seeded = 1;
}


/* 함수 목적: 전역 미션 카탈로그를 디스크에서 다시 로드하도록 강제합니다.
 * 설명:
 *   - 메모리상의 미션 카탈로그(`g_catalog`)가 변경되었거나 외부에서
 *     `data/missions.csv`가 갱신된 경우, 이 함수를 호출하면 내부의
 *     `g_seeded` 플래그를 초기화하고 `ensure_seeded()`를 호출하여 파일로부터
 *     카탈로그를 재구성합니다.
 *   - 간단한 동기화 유틸리티로, 다른 스레드 안전성이나 동시성 제어는
 *     제공하지 않습니다. 멀티스레드 환경에서는 호출자가 별도 동기화해야
 *     합니다.
 *
 * 매개변수:
 *   - 없음
 *
 * 반환값:
 *   - 현재 카탈로그에 로드된 미션 수 (정수, 0 이상)
 */
int mission_refresh_catalog(void) {
    g_seeded = 0;
    ensure_seeded();
    return g_catalog_count;
}

/* 함수 목적: 새로운 미션을 전역 미션 카탈로그에 생성하고 영속화합니다.
 * 설명:
 *   - 전달된 `Mission` 구조체 정보를 바탕으로 내부 전역 카탈로그
 *     (`g_catalog`)에 미션을 추가합니다.
 *   - 중복 이름(name)이 존재하거나 카탈로그가 가득 찬 경우 생성을 거부합니다.
 *   - 생성 시 자동으로 고유 ID(`g_next_id`)를 할당하며, `data/missions.csv`
 *     파일에 "CREATE,<id>,<name>,<type>,<reward>,<ts>" 형식으로 영속화합니다.
 *   - 파일 쓰기 동작은 단순 append 방식이며, 기존 파일 끝에 개행이 없으면
 *     개행을 추가한 뒤 CREATE 라인을 기록합니다.
 *
 * 매개변수:
 *   - m: 생성할 미션 정보를 가진 포인터 (NULL이면 실패)
 *
 * 반환값:
 *   - 성공: 1 (카탈로그에 추가 및 파일에 영속화 완료)
 *   - 실패: 0 (인자 오류, 중복 이름, 카탈로그 포화 등)
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

/* 함수 목적: 열린(미완료) 미션 목록을 제공하여 호출자에게 복사합니다.
 * 설명:
 *   - 내부 전역 미션 카탈로그를 로드(`ensure_seeded()`)한 뒤, 아직
 *     `completed` 플래그가 없는(미완료) 미션들을 `out_arr`에 복사합니다.
 *   - 호출자는 `out_arr`가 충분한 크기(예: `MAX_MISSIONS`)를 가지고 있음을
 *     보장해야 하며, `out_n`에는 전달 가능한 최대 슬롯 수를 넣어 호출합니다.
 *   - 함수는 실제로 복사된 항목 수를 `*out_n`에 저장합니다.
 *
 * 매개변수:
 *   - out_arr: 미션 항목을 저장할 사용자 제공 배열 포인터
 *   - out_n: 호출 시 저장 가능한 최대 항목 수를 담은 포인터; 반환 시
 *            실제 복사한 항목 수가 저장됩니다.
 *
 * 반환값:
 *   - 성공: 1 (복사 및 out_n 설정 완료)
 *   - 실패: 0 (잘못된 인자 등)
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

/* 함수 목적: 특정 사용자(User)의 할당된 미션 목록에서 지정한 ID를 찾아 반환합니다.
 * 설명:
 *   - 주어진 `user`의 `missions` 배열을 순회하여 각 항목의 `id`가
 *     `mission_id`와 일치하는지 검사합니다.
 *   - 일치하는 항목을 찾으면 해당 `Mission *`를 반환하고, 없으면 NULL을 반환합니다.
 *   - 사용자 포인터가 NULL이면 즉시 NULL을 반환합니다.
 *
 * 매개변수:
 *   - user: 대상 사용자 포인터 (NULL 허용)
 *   - mission_id: 찾고자 하는 미션의 ID
 *
 * 반환값:
 *   - 성공: 해당 미션을 가리키는 `Mission *`
 *   - 실패/미발견: `NULL`
 */
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

/* 함수 목적: 사용자가 특정 미션을 완료했을 때 상태를 갱신하고 보상을 지급합니다.
 * 설명:
 *   - 전달된 `username`과 `mission_id`로 사용자의 할당된 미션을 찾아
 *     완료 처리합니다. 내부적으로 `ensure_seeded()`로 전역 카탈로그를
 *     보장하고, `find_user_mission()`으로 사용자의 미션을 조회합니다.
 *   - 이미 완료된 미션이거나 미션을 찾을 수 없는 경우 실패합니다.
 *   - 완료 성공 시 다음 작업을 수행합니다:
 *     1) 사용자의 해당 미션 `completed`를 1로 설정하고 관련 카운터를 갱신.
 *     2) 미션 보상(`reward`)을 사용자의 현금(`user->bank.cash`)에 즉시 추가.
 *     3) 거래 로그(`data/txs/<username>.csv`)에 보상 항목을 기록합니다.
 *     4) `user_update_balance(username, user->bank.balance)`를 호출하여
 *        `data/accounts.csv`를 갱신하도록 합니다(현금 필드 포함).
 *     5) 사용자별 미션 파일(`data/missions/<username>.csv`)에
 *        "COMPLETE,<id>,<ts>" 항목을 append 하여 완료를 영속화합니다.
 *
 * 매개변수:
 *   - username: 미션 완료한 사용자 이름 (NULL이면 실패)
 *   - mission_id: 완료 처리할 미션 ID
 *
 * 반환값:
 *   - 성공: 1 (미션 완료 처리 및 보상/로그/영속화 작업 완료)
 *   - 실패: 0 (인자 오류, 사용자 또는 미션 미발견, 이미 완료된 미션 등)
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

/* 함수 목적: 사용자의 미션 파일을 읽어 사용자의 미션 목록을 구성하고 완료 정보를 반영합니다.
 * 설명:
 *   - `data/missions/<username>.csv` 파일을 읽어 역사적인 "COMPLETE" 항목을
 *     수집하고, ASSIGN 항목은 무시합니다. 수집된 COMPLETE 항목만으로
 *     파일을 재작성하여 ASSIGN 기록을 제거합니다.
 *   - 전역 카탈로그가 비어있을 경우 `ensure_seeded()`를 호출하여 로드합니다.
 *   - 이후 전역 카탈로그를 기준으로 사용자의 `missions` 배열을 채우고,
 *     COMPLETE 목록과 비교하여 각 미션의 `completed` 플래그를 설정합니다.
 *
 * 매개변수:
 *   - username: 미션을 로드할 대상 사용자 이름(문자열)
 *   - user: 미션 정보를 채울 `User *` (NULL이면 실패)
 *
 * 반환값:
 *   - 성공: 사용자의 미션 수 (>= 0)
 *   - 실패: -1 (잘못된 인자 등)
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
        buf = NULL;
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
