/*
 * 파일 목적: stock 도메인 헤더 정의
 * 작성자: ChatGPT
 * 작성일: 2024-06-13
 * 수정 이력: 2024-06-13 ChatGPT - 주석 규칙 적용
 */
#ifndef DOMAIN_STOCK_H
#define DOMAIN_STOCK_H

#include "../types.h"

int stock_list(Stock *out_arr, int *out_n);
int stock_deal(const char *username, const char *symbol, int qty, int is_buy);
int stock_pay_dividends(User *user);  // 🔹 배당 지급
bool shop_decrease_stock_csv(const char *item_name);
void stock_maybe_update_by_time(void);


#endif /* DOMAIN_STOCK_H */
