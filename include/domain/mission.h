/*
 * 파일 목적: mission 도메인 헤더 정의
 * 작성자: ChatGPT
 * 작성일: 2024-06-13
 * 수정 이력: 2024-06-13 ChatGPT - 주석 규칙 적용
 */
#ifndef DOMAIN_MISSION_H
#define DOMAIN_MISSION_H

#include "../types.h"

int mission_list_open(Mission *out_arr, int *out_n);
int mission_create(const Mission *m);
int mission_complete(const char *username, int mission_id);
int mission_load_user(const char *username, User *user);
/* Force re-read of data/missions.csv into the in-memory catalog */
int mission_refresh_catalog(void);

#endif /* DOMAIN_MISSION_H */
