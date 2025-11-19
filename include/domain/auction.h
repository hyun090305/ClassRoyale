#ifndef DOMAIN_AUCTION_H
#define DOMAIN_AUCTION_H

#include "../types.h"

int auction_list(Item *out_items, int *out_n);
int auction_deal(const char *username, const Item *item, int bid_price, int buyout);

#endif /* DOMAIN_AUCTION_H */
