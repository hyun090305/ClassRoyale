#ifndef DOMAIN_SHOP_H
#define DOMAIN_SHOP_H

#include "../types.h"

int shop_list(Shop *out_arr, int *out_n);
int shop_buy(const char *username, const Item *item, int qty);
int shop_sell(const char *username, const Item *item, int qty);


#endif /* DOMAIN_SHOP_H */
