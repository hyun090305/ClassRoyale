/*
 * 파일 목적: qotd 도메인 기능 구현
 * 작성자: 박시유
 */
#include "../../include/domain/qotd.h"
#include "../../include/core/csv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Simple QOTD CSV format (pipe-delimited):
 * date|user|question|status\n
 * Fields will have ',' and newlines replaced by spaces to keep format simple.
 */

/* 함수 목적: 입력 문자열에서 CSV에서 문제되는 문자(, , \n, \r)를 전부 공백 ' '으로 바꿔서 출력 버퍼에 넣어주는 함수.
 * 매개변수: in, out, out_sz
 * 반환 값: 없음
 */
static void sanitize_field(const char *in, char *out, size_t out_sz) {
    if (!in || !out || out_sz == 0) return;
    size_t oi = 0;
    for (size_t i = 0; in[i] != '\0' && oi + 1 < out_sz; ++i) {
        char c = in[i];
        if (c == ',' || c == '\n' || c == '\r') c = ' ';
        out[oi++] = c;
    }
    out[oi] = '\0';
}

/* 함수 목적: qotd 를 csv 파일에 저장하기 위한 스토리지 준비를 수행한다.
 * 매개변수: 없음
 * 반환 값: 성공 여부
 */
int qotd_ensure_storage(void) {
    /* ensure data directory exists; file will be created when appending */
    csv_ensure_dir("data");
    FILE *f = fopen("data/qotd.csv", "a");
    if (!f) return 0;
    fclose(f);
    return 1;
}

/* 함수 목적: 특정 날짜에 어떤 유저가 어떤 문제에 대해 어떤 상태를 남겼는지 기록하는 함수.
 * 매개변수: date, user, question, status
 * 반환 값: 성공 여부
 */
int qotd_record_entry(const char *date, const char *user, const char *question, const char *status) {
    if (!date || !user) return 0;
    if (!qotd_ensure_storage()) return 0;
    char sdate[64];
    char suser[256];
    char squestion[512];
    char sstatus[128];
    sanitize_field(date, sdate, sizeof(sdate));
    sanitize_field(user, suser, sizeof(suser));
    sanitize_field(question ? question : "", squestion, sizeof(squestion));
    sanitize_field(status ? status : "", sstatus, sizeof(sstatus));
    /* use pipe delimiter */
    csv_ensure_dir("data");
    FILE *fp = fopen("data/qotd.csv", "a");
    if (!fp) return 0;
    int ok = fprintf(fp, "%s|%s|%s|%s\n", sdate, suser, squestion, sstatus) > 0;
    fclose(fp);
    return ok ? 1 : 0;
}

/* 함수 목적: qotd.csv를 읽어서, 특정 날짜에 QOTD 기록이 있는 유저 이름 목록을(중복 없이) 만듬
 * 매개변수: date, out_users, out_count
 * 반환 값: 성공 여부
 */
int qotd_get_solved_users_for_date(const char *date, char ***out_users, int *out_count) {
    if (!date || !out_users || !out_count) return 0;
    FILE *f = fopen("data/qotd.csv", "r");
    if (!f) {
        *out_users = NULL;
        *out_count = 0;
        return 1; /* no file treated as empty */
    }
    char buf[1024];
    char **users = NULL;
    int ucount = 0;
    while (fgets(buf, sizeof(buf), f) != NULL) {
        size_t r = strlen(buf);
        if (r == 0) continue;
        if (buf[r-1] == '\n') buf[r-1] = '\0';
        /* parse by pipe '|' */
        char *p = buf;
        char *fld_date = strtok(p, "|");
        char *fld_user = strtok(NULL, "|");
        if (!fld_date || !fld_user) continue;
        if (strcmp(fld_date, date) == 0) {
            int dup = 0;
            for (int i = 0; i < ucount; ++i) {
                if (strcmp(users[i], fld_user) == 0) { dup = 1; break; }
            }
            if (!dup) {
                users = realloc(users, sizeof(char*) * (ucount + 1));
                users[ucount++] = strdup(fld_user);
            }
        }
    }
    fclose(f);
    *out_users = users;
    *out_count = ucount;
    return 1;
}

/* Attempt to load today's QOTD from data/qotd_questions.csv.
 * Returns 1 if found and fills *out, 0 if not found or on error.
 */
/* 함수 목적: 당일의 qotd 를 가져오는 함수
 * 매개변수: out
 * 반환 값: 당일의 qotd 를 찾았는지 여부
 */
int qotd_get_today(QOTD *out) {
    if (!out) return 0;
    FILE *f = fopen("data/qotd_questions.csv", "r");
    if (!f) return 0;
    char buf[2048];
    time_t tnow = time(NULL);
    struct tm *tmnow = localtime(&tnow);
    char today[32] = {0};
    if (tmnow) strftime(today, sizeof(today), "%Y-%m-%d", tmnow);

    while (fgets(buf, sizeof(buf), f) != NULL) {
        size_t r = strlen(buf);
        if (r == 0) continue;
        if (buf[r-1] == '\n') buf[r-1] = '\0';
        /* parse either pipe-delimited or comma-delimited rows:
         * name|date|question|right_index|opt1|opt2|opt3
         * or
         * name,date,question,right_index,opt1,opt2,opt3
         */
        char *s = buf;
        char *flds[8] = {0};
        int fi = 0;
        char *tok = NULL;
        char delim[2] = {0};
        if (strchr(s, '|')) delim[0] = '|'; else delim[0] = ',';
        tok = strtok(s, delim);
        while (tok && fi < 7) {
            flds[fi++] = tok;
            tok = strtok(NULL, delim);
        }
        if (fi >= 4) {
            const char *fdate = flds[1] ? flds[1] : "";
            if (today[0] != '\0' && strcmp(fdate, today) == 0) {
                /* found today's entry */
                memset(out, 0, sizeof(*out));
                if (flds[0]) strncpy(out->name, flds[0], sizeof(out->name)-1);
                if (flds[1]) strncpy(out->date, flds[1], sizeof(out->date)-1);
                if (flds[2]) strncpy(out->question, flds[2], sizeof(out->question)-1);
                out->right_index = flds[3] ? atoi(flds[3]) : 0;
                if (fi > 4 && flds[4]) strncpy(out->opt1, flds[4], sizeof(out->opt1)-1);
                if (fi > 5 && flds[5]) strncpy(out->opt2, flds[5], sizeof(out->opt2)-1);
                if (fi > 6 && flds[6]) strncpy(out->opt3, flds[6], sizeof(out->opt3)-1);
                fclose(f);
                return 1;
            }
        }
    }
    fclose(f);
    return 0;
}

/* Mark today's QOTD as solved by appending a record in data/qotd.csv
 * Format: date|username|qotdname|solved\n
 */
/* 함수 목적: qotd 해결했는지 여부를 확인하는 함수
 * 매개변수: username
 * 반환 값: qotd 해결했는지 여부
 */
int qotd_mark_solved(const char *username) {
    if (!username) return 0;
    QOTD q = {0};
    if (!qotd_get_today(&q)) return 0;
    if (!qotd_ensure_storage()) return 0;
    csv_ensure_dir("data");
    FILE *fp = fopen("data/qotd.csv", "a");
    if (!fp) return 0;
    fprintf(fp, "%s|%s|%s|solved\n", q.date[0] ? q.date : "", username, q.name);
    fclose(fp);
    return 1;
}
