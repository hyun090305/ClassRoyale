
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
