/*
 * 파일 목적: shop 도메인 기능 구현
 * 작성자: ChatGPT
 * 작성일: 2024-06-13
 * 수정 이력: 2024-06-13 ChatGPT - 주석 규칙 적용
 */
#include "../../include/domain/shop.h"

#include <stdio.h>
#include <string.h>

#include "../../include/domain/account.h"
#include "../../include/domain/user.h"
#include "../../include/ui/tui_student.h"
#include "../../include/core/csv.h"
#include <time.h>

static Shop g_shop;
static int g_shop_seeded = 0;

/* 함수 목적: ensure_seeded 함수는 shop 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: 없음
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
static void ensure_seeded(void) {
    if (g_shop_seeded) {
        return;
    }

    memset(&g_shop, 0, sizeof(g_shop));
    snprintf(g_shop.name, sizeof(g_shop.name), "%s", "Class");

    FILE *fp = fopen("data/items.csv", "r");
    if (!fp) {
        g_shop.item_count = 0;
        g_shop_seeded = 1;
        return;
    }

    char line[256];
    int idx = 0;
    size_t max_items = sizeof(g_shop.items) / sizeof(g_shop.items[0]);

    while (idx < (int)max_items && fgets(line, sizeof(line), fp)) {
        char name[64];
        int stock;
        int cost;

        /* "이름,재고,가격" 형태의 줄만 파싱 */
        if (sscanf(line, "%63[^,],%d,%d", name, &stock, &cost) != 3) {
            /* 헤더 줄이나 잘못된 줄은 그냥 건너뜀 */
            continue;
        }

        snprintf(g_shop.items[idx].name,
                 sizeof(g_shop.items[idx].name),
                 "%s", name);
        g_shop.items[idx].stock = stock;
        g_shop.items[idx].cost  = cost;

        idx++;
    }

    fclose(fp);
    g_shop.item_count = idx;
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

/* 함수 목적: shop_list 함수는 shop 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: out_arr, out_n
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
int shop_list(Shop *out_arr, int *out_n) {
    ensure_seeded();
    if (!out_arr || !out_n) {
        return 0;
    }
    out_arr[0] = g_shop;
    *out_n = 1;
    return 1;
}

/* 함수 목적: shop_buy 함수는 shop 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: username, item, qty
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
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
    /* Use cash-on-hand instead of deposit balance. Log transaction to CSV. */
    if (user->bank.cash < total_cost) {
        return 0;
    }
    /* decrease user's cash and append tx CSV (reason = item name) */
    {
        /* sanitize reason similar to account_add_tx */
        char reason_safe[128] = "";
        if (store_item->name && store_item->name[0]) {
            snprintf(reason_safe, sizeof(reason_safe), "%s", store_item->name);
            for (size_t i = 0; i < sizeof(reason_safe); ++i) {
                if (reason_safe[i] == ',' || reason_safe[i] == '\n' || reason_safe[i] == '\r') reason_safe[i] = ' ';
                if (reason_safe[i] == '\0') break;
            }
        }
        user->bank.cash -= total_cost;
        /* append CSV row: ts,reason,amount,balance (balance == deposit balance unchanged) */
        csv_ensure_dir("data");
        csv_ensure_dir("data/txs");
        char path[512];
        snprintf(path, sizeof(path), "data/txs/%s.csv", user->name);
        csv_append_row(path, "%ld,%s,%+d,%d", (long)time(NULL), reason_safe[0] ? reason_safe : "", -total_cost, user->bank.balance);
        /* persist accounts.csv so cash change is saved */
        user_update_balance(user->name, user->bank.balance);
    }
    if (store_item->stock >= 0) {
        store_item->stock -= qty;
    }
    Item *owned = find_or_create_user_item(user, store_item->name);
    if (!owned) {
        /* refund to cash and record reason */
        account_grant_cash(user, total_cost, "SHOP_REFUND");
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

/* 함수 목적: shop_sell 함수는 shop 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: username, item, qty
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
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
    /* Give payment as cash-on-hand and log via account_grant_cash (writes CSV) */
    account_grant_cash(user, payment, "SHOP_SELL");
    if (store_item->stock >= 0) {
        store_item->stock += qty;
    }
    return 1;
}


#define ITEMS_CSV_PATH "data/items.csv"
#define ITEMS_TMP_PATH "data/items.tmp"
#define MAX_LINE 256
#define MAX_NAME 64

/* 함수 목적: shop_decrease_stock_csv 함수는 shop 도메인 기능 구현에서 필요한 동작을 수행합니다.
 * 매개변수: item_name
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
bool shop_decrease_stock_csv(const char *item_name) {
    FILE *fp = fopen(ITEMS_CSV_PATH, "r");
    if (!fp) {
        return false;
    }

    FILE *tmp = fopen(ITEMS_TMP_PATH, "w");
    if (!tmp) {
        fclose(fp);
        return false;
    }

    char line[MAX_LINE];
    char name[MAX_NAME];
    int qty, price;
    int found = 0;

    while (fgets(line, sizeof(line), fp)) {
        // name,quantity,price 형식이라고 가정
        // 이름에 공백 없는 경우: "%63[^,],%d,%d"
        // 공백 있는 이름이면 CSV 규칙 더 꼼꼼히 처리해야 함
        if (sscanf(line, "%63[^,],%d,%d", name, &qty, &price) == 3) {
            if (strcmp(name, item_name) == 0) {
                if (qty > 0) {
                    qty--;
                }
                found = 1;
            }
            fprintf(tmp, "%s,%d,%d\n", name, qty, price);
        } else {
            // 파싱 실패한 라인은 원본 그대로 복사
            fputs(line, tmp);
        }
    }

    fclose(fp);
    fclose(tmp);

    if (!found) {
        // 해당 아이템 못 찾았으면 롤백할지 말지는 선택사항
        // 여기선 그냥 파일 교체는 진행
    }

    // 원본 덮어쓰기
    if (remove(ITEMS_CSV_PATH) != 0) {
        // 실패하면 tmp 남겨두고 false 리턴
        return false;
    }
    if (rename(ITEMS_TMP_PATH, ITEMS_CSV_PATH) != 0) {
        return false;
    }

    return true;
}