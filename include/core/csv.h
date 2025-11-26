/*
 * 파일 목적: 핵심 유틸리티 헤더 정의
 * 작성자: ChatGPT
 * 작성일: 2024-06-13
 * 수정 이력: 2024-06-13 ChatGPT - 주석 규칙 적용
 */
#ifndef CORE_CSV_H
#define CORE_CSV_H

#include <stddef.h>

int csv_ensure_dir(const char *path);
int csv_append_row(const char *path, const char *fmt, ...);
int csv_read_last_lines(const char *path, int max_lines, char **out_buf, size_t *out_len);

#endif // CORE_CSV_H
