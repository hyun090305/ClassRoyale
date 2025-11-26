/*
 * 파일 목적: notification 도메인 헤더 정의
 * 작성자: ChatGPT
 * 작성일: 2024-06-13
 * 수정 이력: 2024-06-13 ChatGPT - 주석 규칙 적용
 */
#ifndef DOMAIN_NOTIFICATION_H
#define DOMAIN_NOTIFICATION_H

#include "../types.h"

void notify_push(const char *username, const char *message);
int notify_recent(const char *username, int limit);
/* Compose recent notifications into a buffer (newline separated).
 * Caller must provide buf size in buflen. Returns number of bytes written or -1 on error. */
int notify_recent_to_buf(const char *username, int limit, char *buf, size_t buflen);

#endif /* DOMAIN_NOTIFICATION_H */
