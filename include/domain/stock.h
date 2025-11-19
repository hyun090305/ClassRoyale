#ifndef DOMAIN_STOCK_H
#define DOMAIN_STOCK_H

#include "types.h"

int stock_list(Stock *out_arr, int *out_n);
int stock_deal(const char *username, const char *symbol, int qty, int is_buy);

#endif /* DOMAIN_STOCK_H */
