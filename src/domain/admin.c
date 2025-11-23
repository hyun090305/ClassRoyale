#include "../../include/domain/admin.h"

#include <stddef.h>
#include <string.h>
#include <time.h>

#include "../../include/core/csv.h"

#include "../../include/domain/mission.h"
#include "../../include/domain/notification.h"
#include "../../include/domain/user.h"

int admin_assign_mission(const char *username, const Mission *m) {
    if (!username || !m) {
        return 0;
    }
    User *user = user_lookup(username);
    if (!user) {
        return 0;
    }
    if (user->mission_count >= (int)(sizeof(user->missions) / sizeof(user->missions[0]))) {
        return 0;
    }
    Mission *slot = &user->missions[user->mission_count++];
    *slot = *m;
    slot->completed = 0;
    user->total_missions += 1;
    /* NOTE: ASSIGN entries are no longer persisted to per-user CSVs. */
    return 1;
}

void admin_message_all(const char *message) {
    if (!message) {
        return;
    }
    size_t count = user_count();
    for (size_t i = 0; i < count; ++i) {
        const User *entry = user_at(i);
        if (entry) {
            notify_push(entry->name, message);
        }
    }
}
