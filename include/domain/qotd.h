/*
 * 파일 목적: qotd 도메인 헤더 정의
 * 작성자: ChatGPT
 * 작성일: 2024-06-13
 * 수정 이력: 2024-06-13 ChatGPT - 주석 규칙 적용
 */
#ifndef DOMAIN_QOTD_H
#define DOMAIN_QOTD_H

#include <stddef.h>

/* QOTD representation for question-lookup APIs */
typedef struct {
	char name[64];
	char date[32]; /* YYYY-MM-DD */
	char question[512];
	int right_index; /* 1..3 */
	char opt1[128];
	char opt2[128];
	char opt3[128];
} QOTD;

int qotd_ensure_storage(void);
int qotd_record_entry(const char *date, const char *user, const char *question, const char *status);
/* returns 1 on success, 0 on error. On success *out_users is an array of malloc'd strings, caller must free each and free the array. */
int qotd_get_solved_users_for_date(const char *date, char ***out_users, int *out_count);

/* Domain helpers to access today's QOTD and mark it solved for a user. */
int qotd_get_today(QOTD *out);
int qotd_mark_solved(const char *username);

#endif /* DOMAIN_QOTD_H */
