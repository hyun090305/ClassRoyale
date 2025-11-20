#include "../../include/domain/shop.h"

#include <stdio.h>
#include <string.h>

#include "../../include/domain/account.h"
#include "../../include/domain/user.h"

static Shop g_shop;
static int g_shop_seeded = 0;

static void ensure_seeded(void) {
    if (g_shop_seeded) {
        return;
    }
    memset(&g_shop, 0, sizeof(g_shop));
    snprintf(g_shop.name, sizeof(g_shop.name), "%s", "Class Shop");
    g_shop.item_count = 3;
    snprintf(g_shop.items[0].name, sizeof(g_shop.items[0].name), "%s", "Pencil");
    g_shop.items[0].stock = 20;
    g_shop.items[0].cost = 100;
    snprintf(g_shop.items[1].name, sizeof(g_shop.items[1].name), "%s", "Notebook");
    g_shop.items[1].stock = 15;
    g_shop.items[1].cost = 200;
    snprintf(g_shop.items[2].name, sizeof(g_shop.items[2].name), "%s", "Badge");
    g_shop.items[2].stock = 5;
    g_shop.items[2].cost = 500;
    g_shop_seeded = 1;
}

static Item *find_store_item(const char *name) {
    for (int i = 0; i < g_shop.item_count; ++i) {
        if (strncmp(g_shop.items[i].name, name, sizeof(g_shop.items[i].name)) == 0) {
            return &g_shop.items[i];
        }
    }
    return NULL;
}

static Item *find_user_item(User *user, const char *name) {
    if (!user) {
        return NULL;
    }
    for (int i = 0; i < (int)(sizeof(user->items) / sizeof(user->items[0])); ++i) {
        if (user->items[i].stock > 0 && strncmp(user->items[i].name, name, sizeof(user->items[i].name)) == 0) {
            return &user->items[i];
        }
    }
    return NULL;
}

static Item *find_or_create_user_item(User *user, const char *name) {
    Item *item = find_user_item(user, name);
    if (item) {
        return item;
    }
    for (int i = 0; i < (int)(sizeof(user->items) / sizeof(user->items[0])); ++i) {
        if (user->items[i].stock == 0) {
            snprintf(user->items[i].name, sizeof(user->items[i].name), "%s", name);
            user->items[i].cost = 0;
            return &user->items[i];
        }
    }
    return NULL;
}

int shop_list(Shop *out_arr, int *out_n) {
    ensure_seeded();
    if (!out_arr || !out_n) {
        return 0;
    }
    out_arr[0] = g_shop;
    *out_n = 1;
    return 1;
}

int shop_buy(const char *username, const Item *item, int qty) {
    ensure_seeded();
    if (!username || !item || qty <= 0) {
        return 0;
    }
    User *user = user_lookup(username);
    if (!user) {
        return 0;
    }
    Item *store_item = find_store_item(item->name);
    if (!store_item) {
        return 0;
    }
    if (store_item->stock >= 0 && store_item->stock < qty) {
        return 0;
    }
    int total_cost = store_item->cost * qty;
    if (!account_adjust(&user->bank, -total_cost)) {
        return 0;
    }
    if (store_item->stock >= 0) {
        store_item->stock -= qty;
    }
    Item *owned = find_or_create_user_item(user, store_item->name);
    if (!owned) {
        account_adjust(&user->bank, total_cost);
        return 0;
    }
    owned->stock += qty;
    owned->cost = store_item->cost;
    g_shop.income += total_cost;
    for (int i = 0; i < g_shop.item_count; ++i) {
        if (&g_shop.items[i] == store_item) {
            g_shop.sales[i] += qty;
            break;
        }
    }
    return 1;
}

int shop_sell(const char *username, const Item *item, int qty) {
    ensure_seeded();
    if (!username || !item || qty <= 0) {
        return 0;
    }
    User *user = user_lookup(username);
    if (!user) {
        return 0;
    }
    Item *owned = find_user_item(user, item->name);
    if (!owned || owned->stock < qty) {
        return 0;
    }
    Item *store_item = find_store_item(item->name);
    if (!store_item) {
        if (g_shop.item_count >= (int)(sizeof(g_shop.items) / sizeof(g_shop.items[0]))) {
            return 0;
        }
        store_item = &g_shop.items[g_shop.item_count++];
        memset(store_item, 0, sizeof(*store_item));
        snprintf(store_item->name, sizeof(store_item->name), "%s", item->name);
    }
    owned->stock -= qty;
    int payment = store_item->cost * qty;
    account_adjust(&user->bank, payment);
    if (store_item->stock >= 0) {
        store_item->stock += qty;
    }
    return 1;
}
