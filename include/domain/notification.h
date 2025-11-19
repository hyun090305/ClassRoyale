#ifndef DOMAIN_NOTIFICATION_H
#define DOMAIN_NOTIFICATION_H

#include "../types.h"

void notify_push(const char *username, const char *message);
int notify_recent(const char *username, int limit);

#endif /* DOMAIN_NOTIFICATION_H */
