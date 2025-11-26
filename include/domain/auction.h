/*
 * 파일 목적: auction 도메인 헤더 정의
 * 작성자: ChatGPT
 * 작성일: 2024-06-13
 * 수정 이력: 2024-06-13 ChatGPT - 주석 규칙 적용
 */
#ifndef DOMAIN_AUCTION_H
#define DOMAIN_AUCTION_H

#include "../types.h"

int auction_list(Item *out_items, int *out_n);
int auction_deal(const char *username, const Item *item, int bid_price, int buyout);

#endif /* DOMAIN_AUCTION_H */
