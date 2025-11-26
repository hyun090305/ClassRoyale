
#ifndef DOMAIN_NOTIFICATION_H
#define DOMAIN_NOTIFICATION_H

#include "../types.h"

void notify_push(const char *username, const char *message);
int notify_recent(const char *username, int limit);
/* Compose recent notifications into a buffer (newline separated).
 * Caller must provide buf size in buflen. Returns number of bytes written or -1 on error. */
int notify_recent_to_buf(const char *username, int limit, char *buf, size_t buflen);

#endif /* DOMAIN_NOTIFICATION_H */
