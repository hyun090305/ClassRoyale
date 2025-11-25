#ifndef DOMAIN_STOCK_H
#define DOMAIN_STOCK_H

#include "../types.h"

int stock_list(Stock *out_arr, int *out_n);
int stock_deal(const char *username, const char *symbol, int qty, int is_buy);
int stock_pay_dividends(User *user);  // 🔹 배당 지급
bool shop_decrease_stock_csv(const char *item_name);


#endif /* DOMAIN_STOCK_H */
