/*
 * 파일 목적: admin 도메인 기능 구현
 * 작성자: ChatGPT
 * 작성일: 2024-06-13
 * 수정 이력: 2024-06-13 ChatGPT - 주석 규칙 적용
 */
#include "../../include/domain/admin.h"

#include <stddef.h>
#include <string.h>
#include <time.h>

#include "../../include/core/csv.h"

#include "../../include/domain/mission.h"
#include "../../include/domain/notification.h"
#include "../../include/domain/user.h"

/* 함수 목적: admin_assign_mission 함수는 admin 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: username, m
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
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

/* 함수 목적: admin_message_all 함수는 admin 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: message
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
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
