/*
 * 파일 목적: 핵심 유틸리티 및 데이터 처리 구현
 * 작성자: ChatGPT
 * 작성일: 2024-06-13
 * 수정 이력: 2024-06-13 ChatGPT - 주석 규칙 적용
 */
#include "../../include/core/csv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#if defined(_WIN32)
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

/* 함수 목적: csv_ensure_dir 함수는 핵심 유틸리티 및 데이터 처리 구현에서 필요한 동작을 수행합니다.
 * 매개변수: path
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
int csv_ensure_dir(const char *path) {
    if (!path) return 0;
    /* try to create; ignore errors if exists */
    MKDIR(path);
    return 1;
}

/* 함수 목적: csv_append_row 함수는 핵심 유틸리티 및 데이터 처리 구현에서 필요한 동작을 수행합니다.
 * 매개변수: path, fmt, ...
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
int csv_append_row(const char *path, const char *fmt, ...) {
    if (!path || !fmt) return 0;
    FILE *f = fopen(path, "a");
    if (!f) return 0;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, "\n");
    fclose(f);
    return 1;
}

/* 함수 목적: csv_read_last_lines 함수는 핵심 유틸리티 및 데이터 처리 구현에서 필요한 동작을 수행합니다.
 * 매개변수: path, max_lines, out_buf, out_len
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
int csv_read_last_lines(const char *path, int max_lines, char **out_buf, size_t *out_len) {
    if (!path || !out_buf || !out_len) return 0;
    FILE *f = fopen(path, "r");
    if (!f) {
        *out_buf = NULL;
        *out_len = 0;
        return 0;
    }
    char *lines[1024];
    int count = 0;
    size_t cap = 0;
    char tmp[1024];
    while (fgets(tmp, sizeof(tmp), f) != NULL) {
        if (count < (int) (sizeof(lines)/sizeof(lines[0]))) {
            lines[count++] = strdup(tmp);
        } else {
            free(lines[0]);
            lines[0] = NULL;
            memmove(lines, lines+1, (count-1)*sizeof(char*));
            lines[count-1] = strdup(tmp);
        }
    }
    fclose(f);

    int start = 0;
    if (max_lines > 0 && count > max_lines) start = count - max_lines;

    /* compute total length */
    for (int i = start; i < count; ++i) {
        cap += strlen(lines[i]) + 1;
    }
    char *buf = malloc(cap + 1);
    if (!buf) {
        for (int i = 0; i < count; ++i) {
            free(lines[i]);
            lines[i] = NULL;
        }
        return 0;
    }
    buf[0] = '\0';
    for (int i = start; i < count; ++i) {
        strcat(buf, lines[i]);
        /* ensure newline at end */
        size_t L = strlen(buf);
        if (L == 0 || buf[L-1] != '\n') strcat(buf, "\n");
        free(lines[i]);
        lines[i] = NULL;
    }

    *out_buf = buf;
    *out_len = strlen(buf);
    return 1;
}
