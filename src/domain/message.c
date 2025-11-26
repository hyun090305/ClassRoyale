/*
 * 파일 목적: message 도메인 기능 구현
 * 작성자: ChatGPT
 * 작성일: 2024-06-13
 * 수정 이력: 2024-06-13 ChatGPT - 주석 규칙 적용
 */
#include "../../include/domain/message.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "../../include/core/csv.h"
#include "../../include/domain/notification.h"
#include "../../include/domain/user.h"

#define MESSAGE_DIR "data/messages"

/* 함수 목적: sanitize_text 함수는 message 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: src, dst, dst_len
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
static void sanitize_text(const char *src, char *dst, size_t dst_len) {
    if (!dst || dst_len == 0) return;
    dst[0] = '\0';
    if (!src) return;
    size_t di = 0;
    for (size_t i = 0; src[i] != '\0' && di + 1 < dst_len; ++i) {
        char ch = src[i];
        if (ch == '\n' || ch == '\r' || ch == ',') {
            ch = ' ';
        }
        dst[di++] = ch;
    }
    dst[di] = '\0';
}

/* 함수 목적: format_relative_time 함수는 message 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: ts, buf, buflen
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
static void format_relative_time(long ts, char *buf, size_t buflen) {
    if (!buf || buflen == 0) return;
    if (ts <= 0) {
        snprintf(buf, buflen, "unknown");
        return;
    }
    time_t now = time(NULL);
    time_t t = (time_t)ts;
    long diff = (long)(now - t);
    if (diff < 0) {
        struct tm *tmv = localtime(&t);
        if (tmv) {
            strftime(buf, buflen, "%Y-%m-%d %H:%M", tmv);
        } else {
            snprintf(buf, buflen, "%ld", ts);
        }
    } else if (diff < 60) {
        snprintf(buf, buflen, "just now");
    } else if (diff < 3600) {
        snprintf(buf, buflen, "%ld minutes ago", diff / 60);
    } else if (diff < 86400) {
        snprintf(buf, buflen, "%ld hours ago", diff / 3600);
    } else {
        struct tm *tmv = localtime(&t);
        if (tmv) {
            strftime(buf, buflen, "%Y-%m-%d %H:%M", tmv);
        } else {
            snprintf(buf, buflen, "%ld", ts);
        }
    }
}

/* 함수 목적: ensure_user_exists 함수는 message 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: username
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
static int ensure_user_exists(const char *username) {
    return username && *username && user_lookup(username) != NULL;
}

/* 함수 목적: message_send 함수는 message 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: from, to, body
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
int message_send(const char *from, const char *to, const char *body) {
    if (!from || !to || !body) return 0;
    if (*from == '\0' || *to == '\0' || *body == '\0') return 0;
    if (strcmp(from, to) == 0) {
        /* allow sending to self? treat as error to prevent confusion */
        return 0;
    }
    if (!ensure_user_exists(from) || !ensure_user_exists(to)) {
        return 0;
    }

    csv_ensure_dir(MESSAGE_DIR);

    char sanitized[MAX_MESSAGE_TEXT];
    sanitize_text(body, sanitized, sizeof(sanitized));
    if (sanitized[0] == '\0') {
        return 0;
    }

    long ts = time(NULL);
    char sender_path[512];
    char recipient_path[512];
    snprintf(sender_path, sizeof(sender_path), "%s/%s.csv", MESSAGE_DIR, from);
    snprintf(recipient_path, sizeof(recipient_path), "%s/%s.csv", MESSAGE_DIR, to);

    int ok_sender = csv_append_row(sender_path, "%ld,S,%s,%s", ts, to, sanitized);
    int ok_recipient = csv_append_row(recipient_path, "%ld,R,%s,%s", ts, from, sanitized);
    if (!ok_sender || !ok_recipient) {
        return 0;
    }

    char note[160];
    snprintf(note, sizeof(note), "New message from %s", from);
    notify_push(to, note);
    return 1;
}

/* 함수 목적: format_feed_line 함수는 message 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: dst, dst_len, ts, dir, other, message
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
static int format_feed_line(char *dst, size_t dst_len, long ts, char dir, const char *other, const char *message) {
    if (!dst || dst_len == 0) return 0;
    char timestr[64];
    format_relative_time(ts, timestr, sizeof(timestr));
    const char *prefix = (dir == 'S') ? "To" : "From";
    int wrote = snprintf(dst, dst_len, "[%s] %s %s: %s", timestr, prefix, other ? other : "?", message ? message : "");
    if (wrote < 0) return 0;
    if ((size_t)wrote >= dst_len) {
        dst[dst_len - 1] = '\0';
        return (int)(dst_len - 1);
    }
    return wrote;
}

/* 함수 목적: parse_line 함수는 message 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: line, out_ts, out_dir, other, other_len, message, msg_len
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
static int parse_line(char *line, long *out_ts, char *out_dir, char *other, size_t other_len, char *message, size_t msg_len) {
    if (!line) return 0;
    /* line format: ts,dir,other,message */
    char *ts_tok = strtok(line, ",");
    char *dir_tok = strtok(NULL, ",");
    char *other_tok = strtok(NULL, ",");
    char *msg_tok = strtok(NULL, "\n");
    if (!ts_tok || !dir_tok || !other_tok || !msg_tok) {
        return 0;
    }
    if (out_ts) *out_ts = atol(ts_tok);
    if (out_dir) *out_dir = dir_tok[0];
    if (other && other_len > 0) {
        snprintf(other, other_len, "%s", other_tok);
    }
    if (message && msg_len > 0) {
        snprintf(message, msg_len, "%s", msg_tok);
    }
    return 1;
}

/* 함수 목적: message_recent_to_buf 함수는 message 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: username, limit, buf, buflen
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
int message_recent_to_buf(const char *username, int limit, char *buf, size_t buflen) {
    if (!username || !buf || buflen == 0) return -1;
    buf[0] = '\0';
    if (limit <= 0) return 0;

    char path[512];
    snprintf(path, sizeof(path), "%s/%s.csv", MESSAGE_DIR, username);

    char *tmp = NULL;
    size_t tmplen = 0;
    if (!csv_read_last_lines(path, limit, &tmp, &tmplen) || !tmp) {
        return 0;
    }

    size_t outpos = 0;
    char *p = tmp;
    while (p && *p && outpos + 1 < buflen) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = '\0';
        char linebuf[512];
        snprintf(linebuf, sizeof(linebuf), "%s", p);
        long ts = 0;
        char dir = 'S';
        char other[64];
        char message[256];
        other[0] = '\0';
        message[0] = '\0';
        if (parse_line(linebuf, &ts, &dir, other, sizeof(other), message, sizeof(message))) {
            char formatted[512];
            if (!format_feed_line(formatted, sizeof(formatted), ts, dir, other, message)) {
                formatted[0] = '\0';
            }
            size_t len = strlen(formatted);
            if (outpos + len + 1 >= buflen) {
                len = buflen - outpos - 2;
            }
            if (len > 0) {
                memcpy(buf + outpos, formatted, len);
                outpos += len;
            }
            if (outpos + 1 < buflen) {
                buf[outpos++] = '\n';
            }
        }
        if (!nl) break;
        p = nl + 1;
    }
    if (outpos >= buflen) outpos = buflen - 1;
    buf[outpos] = '\0';
    free(tmp);
    tmp = NULL;
    return (int)outpos;
}

/* 함수 목적: message_thread_to_buf 함수는 message 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: username, peer, limit, buf, buflen
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
int message_thread_to_buf(const char *username, const char *peer, int limit, char *buf, size_t buflen) {
    if (!username || !peer || !buf || buflen == 0) return -1;
    buf[0] = '\0';
    if (limit <= 0) return 0;

    char path[512];
    snprintf(path, sizeof(path), "%s/%s.csv", MESSAGE_DIR, username);
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return 0;
    }
    typedef struct {
        long ts;
        char dir;
        char message[256];
    } Entry;
    Entry entries[256];
    int count = 0;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char linecopy[512];
        snprintf(linecopy, sizeof(linecopy), "%s", line);
        long ts = 0;
        char dir = 'S';
        char other[64];
        char message[256];
        other[0] = '\0';
        message[0] = '\0';
        if (!parse_line(linecopy, &ts, &dir, other, sizeof(other), message, sizeof(message))) {
            continue;
        }
        if (strncmp(other, peer, sizeof(other)) != 0) {
            continue;
        }
        if (count < (int)(sizeof(entries)/sizeof(entries[0]))) {
            entries[count].ts = ts;
            entries[count].dir = dir;
            snprintf(entries[count].message, sizeof(entries[count].message), "%s", message);
            count++;
        } else {
            /* shift left when buffer is full */
            memmove(entries, entries + 1, (count - 1) * sizeof(Entry));
            entries[count - 1].ts = ts;
            entries[count - 1].dir = dir;
            snprintf(entries[count - 1].message, sizeof(entries[count - 1].message), "%s", message);
        }
    }
    fclose(fp);

    int start = 0;
    if (count > limit) start = count - limit;
    size_t outpos = 0;
    for (int i = start; i < count && outpos + 1 < buflen; ++i) {
        char formatted[512];
        if (!format_feed_line(formatted, sizeof(formatted), entries[i].ts, entries[i].dir, peer, entries[i].message)) {
            formatted[0] = '\0';
        }
        size_t len = strlen(formatted);
        if (outpos + len + 1 >= buflen) {
            len = buflen - outpos - 2;
        }
        if (len > 0) {
            memcpy(buf + outpos, formatted, len);
            outpos += len;
        }
        if (outpos + 1 < buflen) {
            buf[outpos++] = '\n';
        }
    }
    if (outpos >= buflen) outpos = buflen - 1;
    buf[outpos] = '\0';
    return (int)outpos;
}

/* 함수 목적: message_list_partners 함수는 message 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: username, partners[][50], max_partners
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
int message_list_partners(const char *username, char partners[][50], int max_partners) {
    if (!username || !partners || max_partners <= 0) return 0;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.csv", MESSAGE_DIR, username);
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return 0;
    }
    int count = 0;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char linecopy[512];
        snprintf(linecopy, sizeof(linecopy), "%s", line);
        long ts = 0;
        char dir = 'S';
        char other[64];
        char message[256];
        if (!parse_line(linecopy, &ts, &dir, other, sizeof(other), message, sizeof(message))) {
            continue;
        }
        int exists = 0;
        for (int i = 0; i < count; ++i) {
            if (strncmp(partners[i], other, 50) == 0) {
                exists = 1;
                break;
            }
        }
        if (!exists && count < max_partners) {
            snprintf(partners[count], 50, "%s", other);
            count++;
        }
    }
    fclose(fp);
    return count;
}
