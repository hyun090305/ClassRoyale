/*
 * 파일 목적: message 도메인 헤더 정의
 * 작성자: ChatGPT
 * 작성일: 2024-06-13
 * 수정 이력: 2024-06-13 ChatGPT - 주석 규칙 적용
 */
#ifndef DOMAIN_MESSAGE_H
#define DOMAIN_MESSAGE_H

#include "../types.h"

#define MAX_MESSAGE_TEXT 256

typedef struct PrivateMessage {
    long timestamp;
    char sender[50];
    char recipient[50];
    char body[MAX_MESSAGE_TEXT];
} PrivateMessage;

int message_send(const char *from, const char *to, const char *body);
int message_recent_to_buf(const char *username, int limit, char *buf, size_t buflen);
int message_thread_to_buf(const char *username, const char *peer, int limit, char *buf, size_t buflen);
int message_list_partners(const char *username, char partners[][50], int max_partners);

#endif /* DOMAIN_MESSAGE_H */
