
#ifndef DOMAIN_ADMIN_H
#define DOMAIN_ADMIN_H

#include "../types.h"

int admin_assign_mission(const char *username, const Mission *m);
void admin_message_all(const char *message);

#endif /* DOMAIN_ADMIN_H */
