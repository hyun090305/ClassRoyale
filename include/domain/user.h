/*
 * 파일 목적: user 도메인 헤더 정의
 * 작성자: ChatGPT
 * 작성일: 2024-06-13
 * 수정 이력: 2024-06-13 ChatGPT - 주석 규칙 적용
 */
#ifndef DOMAIN_USER_H
#define DOMAIN_USER_H

#include "../types.h"

int user_register(const User *new_user);
int user_auth(const char *username, const char *password);
User *user_lookup(const char *username);
size_t user_count(void);
const User *user_at(size_t index);
int user_update_balance(const char *username, int new_balance);

#endif /* DOMAIN_USER_H */
