#include "../../include/domain/qotd.h"
#include "../../include/core/csv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Simple QOTD CSV format (pipe-delimited):
 * date|user|question|status\n
 * Fields will have '|' and newlines replaced by spaces to keep format simple.
 */

static void sanitize_field(const char *in, char *out, size_t out_sz) {
    if (!in || !out || out_sz == 0) return;
    size_t oi = 0;
    for (size_t i = 0; in[i] != '\0' && oi + 1 < out_sz; ++i) {
        char c = in[i];
        if (c == '|' || c == '\n' || c == '\r') c = ' ';
        out[oi++] = c;
    }
    out[oi] = '\0';
}

int qotd_ensure_storage(void) {
    /* ensure data directory exists; file will be created when appending */
    csv_ensure_dir("data");
    FILE *f = fopen("data/qotd.csv", "a");
    if (!f) return 0;
    fclose(f);
    return 1;
}

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
    return csv_append_row("data/qotd.csv", "%s|%s|%s|%s", sdate, suser, squestion, sstatus);
}

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
        /* parse by pipe */
        char *fld_date = strtok(buf, "|");
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
