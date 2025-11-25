#include "../../include/domain/notification.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "../../include/core/csv.h"

#include "../../include/domain/user.h"

typedef struct Notification {
    char username[50];
    char message[128];
} Notification;

static Notification g_notifications[MAX_NOTIFICATIONS];
static int g_notification_count = 0;
static int g_notification_cursor = 0;

void notify_push(const char *username, const char *message) {
    if (!username || !message) {
        return;
    }
    Notification *slot = &g_notifications[g_notification_cursor];
    snprintf(slot->username, sizeof(slot->username), "%s", username);
    snprintf(slot->message, sizeof(slot->message), "%s", message);
    if (g_notification_count < MAX_NOTIFICATIONS) {
        g_notification_count++;
    }
    g_notification_cursor = (g_notification_cursor + 1) % MAX_NOTIFICATIONS;
    /* persist to per-user CSV */
    csv_ensure_dir("data/notifications");
    char path[512];
    snprintf(path, sizeof(path), "data/notifications/%s.csv", username);
    time_t ts = time(NULL);
    /* append: timestamp, message */
    /* sanitize newlines in message */
    char msgbuf[256];
    strncpy(msgbuf, message, sizeof(msgbuf)-1);
    msgbuf[sizeof(msgbuf)-1] = '\0';
    for (char *p = msgbuf; *p; ++p) if (*p == '\n' || *p == '\r') *p = ' ';
    csv_append_row(path, "%lld,%s", (long long)ts, msgbuf);
}

int notify_recent(const char *username, int limit) {
    if (!username || limit <= 0) {
        return 0;
    }
    int shown = 0;
    for (int i = 0; i < g_notification_count && shown < limit; ++i) {
        int index = (g_notification_cursor - 1 - i + MAX_NOTIFICATIONS) % MAX_NOTIFICATIONS;
        if (strncmp(g_notifications[index].username, username, sizeof(g_notifications[index].username)) == 0) {
            printf("[%s] %s\n", g_notifications[index].username, g_notifications[index].message);
            shown++;
        }
    }
    return shown;
}

int notify_recent_to_buf(const char *username, int limit, char *buf, size_t buflen) {
    if (!username || !buf || buflen == 0) return -1;
    char path[512];
    snprintf(path, sizeof(path), "data/notifications/%s.csv", username);
    char *tmp = NULL;
    size_t tmplen = 0;
    if (!csv_read_last_lines(path, limit, &tmp, &tmplen) || !tmp) {
        buf[0] = '\0';
        return 0;
    }
    /* tmp contains newline-terminated lines like: "<ts>,<message>\n".
       Parse each line, convert timestamp to human-friendly string and
       append to `buf`. Return number of bytes written. */
    size_t outpos = 0;
    char *p = tmp;
    time_t now = time(NULL);
    while (p && *p && outpos + 1 < buflen) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = '\0';
        /* split "ts,msg" */
        char *comma = strchr(p, ',');
        time_t ts = 0;
        char *msg = NULL;
        if (comma) {
            *comma = '\0';
            ts = (time_t)atoll(p);
            msg = comma + 1;
        } else {
            msg = p;
        }

        /* format relative time in Korean */
        char timestr[64];
        if (ts <= 0) {
            snprintf(timestr, sizeof(timestr), "unknown");
        } else {
            long diff = (long)(now - ts);
            if (diff < 0) {
                /* future? show absolute */
                struct tm *tm = localtime(&ts);
                if (tm) strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M", tm);
                else snprintf(timestr, sizeof(timestr), "%ld", ts);
            } else if (diff < 60) {
                snprintf(timestr, sizeof(timestr), "just now");
            } else if (diff < 3600) {
                snprintf(timestr, sizeof(timestr), "%ld minutes ago", diff / 60);
            } else if (diff < 86400) {
                snprintf(timestr, sizeof(timestr), "%ld hours ago", diff / 3600);
            } else {
                struct tm *tm = localtime(&ts);
                if (tm) strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M", tm);
                else snprintf(timestr, sizeof(timestr), "%ld", ts);
            }
        }

        /* append formatted line: "[timestr] message\n" */
        int wrote = snprintf(buf + outpos, buflen - outpos, "[%s] %s\n", timestr, msg ? msg : "");
        if (wrote < 0) break;
        if ((size_t)wrote >= buflen - outpos) {
            /* truncated: ensure NUL and break */
            outpos = buflen - 1;
            buf[outpos] = '\0';
            break;
        }
        outpos += (size_t)wrote;

        if (!nl) break;
        p = nl + 1;
    }

    free(tmp);
    /* ensure NUL termination */
    if (outpos >= buflen) outpos = buflen - 1;
    buf[outpos] = '\0';
    return (int)outpos;
}
