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
    long ts = time(NULL);
    /* append: timestamp, message */
    /* sanitize newlines in message */
    char msgbuf[256];
    strncpy(msgbuf, message, sizeof(msgbuf)-1);
    msgbuf[sizeof(msgbuf)-1] = '\0';
    for (char *p = msgbuf; *p; ++p) if (*p == '\n' || *p == '\r') *p = ' ';
    csv_append_row(path, "%ld,%s", ts, msgbuf);
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
    /* tmp contains newline-terminated lines; copy into buf */
    size_t tocopy = tmplen < buflen-1 ? tmplen : buflen-1;
    memcpy(buf, tmp, tocopy);
    buf[tocopy] = '\0';
    free(tmp);
    return (int)tocopy;
}
