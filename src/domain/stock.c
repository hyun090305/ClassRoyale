#include "../../include/domain/stock.h"

#include <stdio.h>
#include <string.h>

#include "../../include/domain/account.h"
#include "../../include/domain/user.h"



static Stock g_stocks[16];
static int   g_stock_count = 0;
static int   g_seeded      = 0;

/* -------------------------------------------------------------------------- */
/*  static 함수 선언 (프로토타입)                                             */
/* -------------------------------------------------------------------------- */

static void          ensure_seeded(void);
static Stock        *find_stock(const char *symbol);
static StockHolding *find_holding(User *user, const char *symbol);
static StockHolding *find_or_create_holding(User *user, const char *symbol);

/* -------------------------------------------------------------------------- */
/*  static helper 함수 정의                                                   */
/* -------------------------------------------------------------------------- */

static void ensure_seeded(void) {
    if (g_seeded) {
        return;
    }

    memset(g_stocks, 0, sizeof(g_stocks));

    // 예시 종목 1: CRX
    snprintf(g_stocks[0].name, sizeof(g_stocks[0].name), "%s", "CRX");
    g_stocks[0].id            = 1;
    g_stocks[0].base_price    = 1000;
    g_stocks[0].current_price = 1200;
    g_stocks[0].previous_price = 0;
    snprintf(g_stocks[0].news, sizeof(g_stocks[0].news),
             "%s", "Class Royale index rising");
    g_stocks[0].dividend_per_tick = 5;   // 주당 5Cr 배당

    // 예시 종목 2: EDU
    snprintf(g_stocks[1].name, sizeof(g_stocks[1].name), "%s", "EDU");
    g_stocks[1].id            = 2;
    g_stocks[1].base_price    = 800;
    g_stocks[1].current_price = 900;
    g_stocks[1].previous_price = 0;
    snprintf(g_stocks[1].news, sizeof(g_stocks[1].news),
             "%s", "Education sector benefits");
    g_stocks[1].dividend_per_tick = 3;   // 주당 3Cr 배당

    g_stock_count = 2;
    g_seeded      = 1;
}

static Stock *find_stock(const char *symbol) {
    for (int i = 0; i < g_stock_count; ++i) {
        if (strncmp(g_stocks[i].name,
                    symbol,
                    sizeof(g_stocks[i].name)) == 0) {
            return &g_stocks[i];
        }
    }
    return NULL;
}

static StockHolding *find_holding(User *user, const char *symbol) {
    if (!user) {
        return NULL;
    }
    for (int i = 0; i < user->holding_count; ++i) {
        if (strncmp(user->holdings[i].symbol,
                    symbol,
                    sizeof(user->holdings[i].symbol)) == 0) {
            return &user->holdings[i];
        }
    }
    return NULL;
}

static StockHolding *find_or_create_holding(User *user, const char *symbol) {
    StockHolding *holding = find_holding(user, symbol);
    if (holding) {
        return holding;
    }

    if (user->holding_count >= MAX_HOLDINGS) {
        return NULL;
    }

    holding = &user->holdings[user->holding_count++];
    memset(holding, 0, sizeof(*holding));
    snprintf(holding->symbol, sizeof(holding->symbol), "%s", symbol);
    return holding;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 리스트, 매매, 배당                                              */
/* -------------------------------------------------------------------------- */

/* 전체 종목 목록을 out_arr에 채워 넣고 개수를 out_n에 넣음 */
int stock_list(Stock *out_arr, int *out_n) {
    ensure_seeded();

    if (!out_arr || !out_n) {
        return 0;
    }

    for (int i = 0; i < g_stock_count; ++i) {
        out_arr[i] = g_stocks[i];
    }
    *out_n = g_stock_count;

    return 1;
}

/*
 * 주식 매매
 *  - username : 유저 이름
 *  - symbol   : 종목 이름 (예: "CRX")
 *  - qty      : 수량 (양수)
 *  - is_buy   : 1 = 매수, 0 = 매도
 *
 * 성공하면 1, 실패하면 0 반환
 */
int stock_deal(const char *username, const char *symbol, int qty, int is_buy) {
    ensure_seeded();

    if (!username || !symbol || qty <= 0) {
        return 0;
    }

    User *user = user_lookup(username);
    if (!user) {
        return 0;
    }

    Stock *stock = find_stock(symbol);
    if (!stock) {
        return 0;
    }

    if (is_buy) {
        int cost = stock->current_price * qty;

        /* 계좌에서 돈 빼기 (잔액 부족 등으로 실패하면 0 반환 가정) */
        if (!account_add_tx(user, -cost, symbol)) {
            return 0;
        }

        StockHolding *holding = find_or_create_holding(user, symbol);
        if (!holding) {
            /* 홀딩 생성 실패 시, 돈 되돌리기 */
            account_add_tx(user, cost, "STOCK_REFUND");
            return 0;
        }
        holding->qty += qty;
    } else {
        StockHolding *holding = find_holding(user, symbol);
        if (!holding || holding->qty < qty) {
            return 0;
        }
        holding->qty -= qty;

        int revenue = stock->current_price * qty;
        account_add_tx(user, revenue, "STOCK_SELL");
    }

    /* 간단한 가격 변동 로직 */
    stock->previous_price = stock->current_price;
    stock->current_price += (is_buy ? 5 : -5);

    return 1;
}

/*
 * 배당 지급:
 *  - 한 유저가 가진 모든 종목에 대해
 *    dividend_per_tick * 보유수량 만큼 입금
 *  - 총 받은 배당금 합계를 반환
 */
int stock_pay_dividends(User *user) {
    ensure_seeded();
    if (!user) {
        return 0;
    }

    int total_dividend = 0;

    for (int i = 0; i < user->holding_count; ++i) {
        StockHolding *holding = &user->holdings[i];
        if (holding->qty <= 0) {
            continue;
        }

        Stock *stock = find_stock(holding->symbol);
        if (!stock) {
            continue;
        }
        if (stock->dividend_per_tick <= 0) {
            continue;
        }

        int amount = stock->dividend_per_tick * holding->qty;
        if (amount <= 0) {
            continue;
        }

        account_add_tx(user, amount, "DIVIDEND");
        total_dividend += amount;
    }

    return total_dividend;
}