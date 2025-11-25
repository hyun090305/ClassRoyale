#include "../../include/domain/stock.h"

#include <stdio.h>
#include <string.h>

#include "../../include/domain/account.h"
#include "../../include/domain/user.h"
#include <time.h>  // 🔹 시간 사용
#include <stdlib.h>

#define MAX_STOCKS 16

static Stock g_stocks[MAX_STOCKS];
static int   g_stock_count  = 0;
static int   g_seeded       = 0;

static time_t g_start_time   = 0;
static int    g_applied_hours = 0;
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

static StockHolding *find_holding(User *user, const char *symbol) {
    if (!user || !symbol) {
        return NULL;
    }

    for (int i = 0; i < user->holding_count; ++i) {
        // 심볼 문자열 비교
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

static void seed_default_stocks(void) {
    /* CSV 없을 때를 대비한 기본값 (원하면 수정/삭제 가능) */
    memset(g_stocks, 0, sizeof(g_stocks));
    g_stock_count = 2;

    Stock *s = &g_stocks[0];
    snprintf(s->name, sizeof(s->name), "CRX");
    s->id            = 1;
    s->log_len       = 2;
    s->log[0]        = 1000;
    s->log[1]        = 1200;
    s->base_price    = s->log[0];
    s->previous_price= s->log[0];
    s->current_price = s->log[1];
    snprintf(s->news, sizeof(s->news),
             "Class Royale index rising");

    s = &g_stocks[1];
    snprintf(s->name, sizeof(s->name), "EDU");
    s->id            = 2;
    s->log_len       = 2;
    s->log[0]        = 800;
    s->log[1]        = 900;
    s->base_price    = s->log[0];
    s->previous_price= s->log[0];
    s->current_price = s->log[1];
    snprintf(s->news, sizeof(s->news),
             "Education sector benefits");
}

static void stock_load_from_csv(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        seed_default_stocks();
        return;
    }

    char line[512];

    /* 1줄째: 시간일 수도 있고 아닐 수도 있음 */
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        seed_default_stocks();
        return;
    }

    /* 첫 줄이 숫자면 "기준 시각"으로 보고, 아니면 그냥 첫 종목으로 취급 */
    char *endptr = NULL;
    long long ts = strtoll(line, &endptr, 10);
    int first_line_used_as_time = 0;

    if (endptr != line && ts > 0) {
        /* 숫자 파싱 성공 → 기준 시각으로 사용 */
        g_start_time = (time_t)ts;
        first_line_used_as_time = 1;
    } else {
        /* 숫자 아니면, 그냥 이 줄도 종목 데이터로 다시 파싱 */
        g_start_time = time(NULL);  // 적당한 값으로
    }

    memset(g_stocks, 0, sizeof(g_stocks));
    g_stock_count = 0;

    /* 만약 첫 줄이 시간 아니었으면, line에 이미 종목 데이터가 들어있으니까
       그 줄부터 다시 처리 */
    if (!first_line_used_as_time) {
        /* line 변수 안에 있는 내용을 그대로 재사용 */
        goto PARSE_LINE_AS_STOCK;
    }

    while (fgets(line, sizeof(line), fp)) {
PARSE_LINE_AS_STOCK:
        if (line[0] == '\n' || line[0] == '\r' || line[0] == '#') {
            continue;
        }

        if (g_stock_count >= MAX_STOCKS) {
            break;
        }

        char *name = strtok(line, ", \t\r\n");
        if (!name) continue;

        Stock *s = &g_stocks[g_stock_count];
        memset(s, 0, sizeof(*s));

        snprintf(s->name, sizeof(s->name), "%s", name);
        s->id = g_stock_count + 1;

        int idx = 0;
        char *token = NULL;
        while ((token = strtok(NULL, ", \t\r\n")) != NULL) {
            if (idx >= 200) break;
            s->log[idx++] = atoi(token);
        }

        if (idx == 0) {
            continue; // 가격 기록이 없으면 무시
        }

        s->log_len       = idx;
        s->base_price    = s->log[0];
        s->current_price = s->log[idx - 1];
        s->previous_price= (idx >= 2) ? s->log[idx - 2] : s->current_price;

        snprintf(s->news, sizeof(s->news),
                 "Loaded %d points of history", s->log_len);

        g_stock_count++;

        /* first_line_used_as_time == 0 인 경우는, 첫 줄 처리 후 플래그 변경하고
           다음부터는 정상적인 while 루프로 들어감 */
        if (!first_line_used_as_time) {
            first_line_used_as_time = 1;
            break;  // 첫 줄 처리 끝났으니 while 루프 다시 진입
        }
    }

    fclose(fp);

    if (g_stock_count == 0) {
        seed_default_stocks();
    }
}

static void ensure_seeded(void) {
    if (g_seeded) return;

    srand((unsigned)time(NULL));       // 🔹 랜덤 시드
    stock_load_from_csv("data/stocks.csv");

    if (g_start_time == 0) {
        g_start_time = time(NULL);
    }
    g_applied_hours = 0;

    g_seeded = 1;
}

static void stock_append_price(Stock *s, int new_price) {
    if (!s) return;

    s->previous_price = s->current_price;
    s->current_price  = new_price;

    if (s->log_len < 200) {
        s->log[s->log_len++] = new_price;
    } else {
        /* 꽉 찼으면 하나씩 밀어버리고 맨 뒤에 추가 */
        for (int i = 1; i < 200; ++i) {
            s->log[i - 1] = s->log[i];
        }
        s->log[199] = new_price;
        s->log_len  = 200;
    }
}

static void stock_random_step(Stock *s) {
    if (!s) return;

    int delta = 0;
    int r = rand() % 3; // 0,1,2
    if (r == 0)      delta = -10;
    else if (r == 1) delta = 0;
    else             delta = 10;

    int new_price = s->current_price + delta;
    if (new_price < 0) new_price = 0;

    stock_append_price(s, new_price);
}

void stock_maybe_update_by_time(void) {
    ensure_seeded();

    time_t now = time(NULL);
    if (g_start_time == 0) {
        g_start_time = now;
    }

    double diff = difftime(now, g_start_time);
    if (diff < 0) diff = 0;

    /* 총 몇 시간이 지났는지 */
    int total_hours = (int)(diff / 3600.0);

    /* 이미 적용한 시간(step)보다 크지 않으면 할 일 없음 */
    if (total_hours <= g_applied_hours) {
        return;
    }

    int new_steps = total_hours - g_applied_hours;
    if (new_steps <= 0) {
        return;
    }

    /* 새로 지난 시간만큼 주가 여러 번 움직이기 */
    for (int step = 0; step < new_steps; ++step) {
        for (int i = 0; i < g_stock_count; ++i) {
            stock_random_step(&g_stocks[i]);
        }
    }

    g_applied_hours = total_hours;
}

static Stock *find_stock(const char *symbol) {
    if (!symbol) return NULL;
    for (int i = 0; i < g_stock_count; ++i) {
        if (strncmp(g_stocks[i].name, symbol,
                    sizeof(g_stocks[i].name)) == 0) {
            return &g_stocks[i];
        }
    }
    return NULL;
}

int stock_list(Stock *out_arr, int *out_n) {
    ensure_seeded();
    if (!out_arr || !out_n) return 0;

    for (int i = 0; i < g_stock_count; ++i) {
        out_arr[i] = g_stocks[i];
    }
    *out_n = g_stock_count;
    return 1;
}

int stock_get_history(const char *symbol, int *out_buf, int max_len) {
    ensure_seeded();
    if (!symbol || !out_buf || max_len <= 0) return 0;

    Stock *s = find_stock(symbol);
    if (!s) return 0;

    int len = s->log_len;
    if (len > max_len) len = max_len;

    for (int i = 0; i < len; ++i) {
        out_buf[i] = s->log[i];
    }
    return len;
}