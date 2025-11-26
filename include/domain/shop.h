/*
 * 파일 목적: shop 도메인 헤더 정의
 * 작성자: ChatGPT
 * 작성일: 2024-06-13
 * 수정 이력: 2024-06-13 ChatGPT - 주석 규칙 적용
 */
#ifndef DOMAIN_SHOP_H
#define DOMAIN_SHOP_H

#include "../types.h"

int shop_list(Shop *out_arr, int *out_n);
int shop_buy(const char *username, const Item *item, int qty);
int shop_sell(const char *username, const Item *item, int qty);


#endif /* DOMAIN_SHOP_H */
