/*
 * 파일 목적: auction 도메인 기능 구현
 * 작성자: ChatGPT
 * 작성일: 2024-06-13
 * 수정 이력: 2024-06-13 ChatGPT - 주석 규칙 적용
 */
#include "../../include/domain/auction.h"

#include <stdio.h>
#include <string.h>

#include "../../include/domain/account.h"
#include "../../include/domain/user.h"

static Item g_items[16];
static int g_item_count = 0;
static int g_seeded = 0;
static int g_high_bids[16];
static char g_high_bidders[16][50];

static Item *find_or_create_user_item(User *user, const char *name) {
    if (!user) {
        return NULL;
    }
    for (int i = 0; i < (int)(sizeof(user->items) / sizeof(user->items[0])); ++i) {
        if (user->items[i].stock > 0 && strncmp(user->items[i].name, name, sizeof(user->items[i].name)) == 0) {
            return &user->items[i];
        }
    }
    for (int i = 0; i < (int)(sizeof(user->items) / sizeof(user->items[0])); ++i) {
        if (user->items[i].stock == 0) {
            snprintf(user->items[i].name, sizeof(user->items[i].name), "%s", name);
            return &user->items[i];
        }
    }
    return NULL;
}

/* 함수 목적: ensure_seeded 함수는 auction 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: 없음
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
static void ensure_seeded(void) {
    if (g_seeded) {
        return;
    }
    memset(g_items, 0, sizeof(g_items));
    snprintf(g_items[0].name, sizeof(g_items[0].name), "%s", "Rare Card");
    g_items[0].cost = 1000;
    g_items[0].stock = 1;

    snprintf(g_items[1].name, sizeof(g_items[1].name), "%s", "Signed Ball");
    g_items[1].cost = 1500;
    g_items[1].stock = 1;

    g_item_count = 2;
    g_seeded = 1;
}

/* 함수 목적: auction_list 함수는 auction 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: out_items, out_n
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
int auction_list(Item *out_items, int *out_n) {
    ensure_seeded();
    if (!out_items || !out_n) {
        return 0;
    }
    for (int i = 0; i < g_item_count; ++i) {
        out_items[i] = g_items[i];
    }
    *out_n = g_item_count;
    return 1;
}

/* 함수 목적: auction_deal 함수는 auction 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: username, item, bid_price, buyout
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
int auction_deal(const char *username, const Item *item, int bid_price, int buyout) {
    ensure_seeded();
    if (!username || !item) {
        return 0;
    }
    User *user = user_lookup(username);
    if (!user) {
        return 0;
    }
    Item *auction_item = NULL;
    for (int i = 0; i < g_item_count; ++i) {
        if (strncmp(g_items[i].name, item->name, sizeof(g_items[i].name)) == 0) {
            auction_item = &g_items[i];
            break;
        }
    }
    if (!auction_item) {
        return 0;
    }
    if (buyout) {
        if (auction_item->stock <= 0) {
            return 0;
        }
        if (!account_add_tx(user, -auction_item->cost, "AUCTION_BUYOUT")) {
            return 0;
        }
        auction_item->stock -= 1;
        Item *owned = find_or_create_user_item(user, auction_item->name);
        if (owned) {
            owned->stock += 1;
            owned->cost = auction_item->cost;
        }
        return 1;
    }
    int index = (int)(auction_item - g_items);
    if (bid_price > g_high_bids[index]) {
        g_high_bids[index] = bid_price;
        snprintf(g_high_bidders[index], sizeof(g_high_bidders[index]), "%s", username);
        return 1;
    }
    return 0;
}
