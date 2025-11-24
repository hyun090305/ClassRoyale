#include "../../include/domain/stock.h"

#include <stdio.h>
#include <string.h>

#include "../../include/domain/account.h"
#include "../../include/domain/user.h"

static Stock g_stocks[16];
static int g_stock_count = 0;
static int g_seeded = 0;

static void ensure_seeded(void) {
    if (g_seeded) {
        return;
    }
    memset(g_stocks, 0, sizeof(g_stocks));
    snprintf(g_stocks[0].name, sizeof(g_stocks[0].name), "%s", "CRX");
    g_stocks[0].id = 1;
    g_stocks[0].base_price = 1000;
    g_stocks[0].current_price = 1200;
    snprintf(g_stocks[0].news, sizeof(g_stocks[0].news), "%s", "Class Royale index rising");

    snprintf(g_stocks[1].name, sizeof(g_stocks[1].name), "%s", "EDU");
    g_stocks[1].id = 2;
    g_stocks[1].base_price = 800;
    g_stocks[1].current_price = 900;
    snprintf(g_stocks[1].news, sizeof(g_stocks[1].news), "%s", "Education sector benefits");

    g_stock_count = 2;
    g_seeded = 1;
}

static Stock *find_stock(const char *symbol) {
    for (int i = 0; i < g_stock_count; ++i) {
        if (strncmp(g_stocks[i].name, symbol, sizeof(g_stocks[i].name)) == 0) {
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
        if (strncmp(user->holdings[i].symbol, symbol, sizeof(user->holdings[i].symbol)) == 0) {
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
        if (!account_add_tx(user, -cost, symbol)) {
            return 0;
        }
        StockHolding *holding = find_or_create_holding(user, symbol);
        if (!holding) {
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
    stock->previous_price = stock->current_price;
    stock->current_price += (is_buy ? 5 : -5);
    return 1;
}
