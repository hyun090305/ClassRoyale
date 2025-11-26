/*
 * 파일 목적: admin 도메인 기능 구현
 * 작성자: 박시유
 */
#include "../../include/domain/admin.h"

#include <stddef.h>
#include <string.h>
#include <time.h>

#include "../../include/core/csv.h"

#include "../../include/domain/mission.h"
#include "../../include/domain/notification.h"
#include "../../include/domain/user.h"

/* 함수 목적: 지정한 사용자(username)에게 미션을 배정합니다.
 * 설명:
 *   - 관리자 또는 시스템이 특정 사용자에게 미션을 추가할 때 호출합니다.
 *   - 사용자 존재 여부와 남아 있는 미션 슬롯을 확인한 뒤, 해당 사용자의
 *     미션 배열에 `Mission` 내용을 복사하고 `completed`를 0으로 초기화합니다.
 *   - 배정 성공 시 `mission_count`와 `total_missions`를 적절히 증가시킵니다.
 *   - 현재 구현은 배정(ASSIGN) 자체를 per-user CSV로 영속화하지 않으므로,
 *     필요하면 호출자 또는 별도 코드에서 영속화 처리를 추가해야 합니다.
 *
 * 매개변수:
 *   - username: 대상 사용자 이름 (NULL이면 실패)
 *   - m: 배정할 `Mission` 포인터 (NULL이면 실패)
 *
 * 반환값:
 *   - 성공: 1 (미션 배정 성공)
 *   - 실패: 0 (인자 오류, 사용자 없음, 미션 슬롯 부족 등)

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

/* 함수 목적: 모든 사용자에게 공지(message)를 전송합니다.
 * 설명:
 *   - 전달된 문자열을 모든 등록 사용자에게 알림으로 푸시합니다.
 *   - 내부적으로 `user_count()`와 `user_at()`을 사용해 등록된 모든
 *     사용자를 순회하며 `notify_push(username, message)`를 호출합니다.
 *   - 메시지는 즉시 전파되며, 별도의 영속화(로그 저장)는 수행하지 않습니다.
 *
 * 매개변수:
 *   - message: 보낼 메시지 문자열 (NULL이면 아무 동작도 수행하지 않음)
 *
 * 반환값:
 *   - 없음 (void)
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
