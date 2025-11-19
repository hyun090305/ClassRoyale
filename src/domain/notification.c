#include "domain/notification.h"

#include <stdio.h>
#include <string.h>

#include "domain/user.h"

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
