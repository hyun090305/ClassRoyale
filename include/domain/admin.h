/*
 * 파일 목적: admin 도메인 헤더 정의
 * 작성자: ChatGPT
 * 작성일: 2024-06-13
 * 수정 이력: 2024-06-13 ChatGPT - 주석 규칙 적용
 */
#ifndef DOMAIN_ADMIN_H
#define DOMAIN_ADMIN_H

#include "../types.h"

int admin_assign_mission(const char *username, const Mission *m);
void admin_message_all(const char *message);

#endif /* DOMAIN_ADMIN_H */
