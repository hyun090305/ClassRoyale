/*
 * 파일 목적: stock 도메인 기능 구현
 * 작성자: 박성우
 */
#include "../../include/domain/stock.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "../../include/domain/account.h"
#include "../../include/domain/user.h"
#include <time.h>
#include <stdlib.h>

#define MAX_STOCKS 16
#define STOCK_STEP_SECONDS 600
// 최대 거래 내역 개수
static Stock g_stocks[MAX_STOCKS];
// 현재 등록된 주식 수
static int   g_stock_count  = 0;
// 시드 초기화 여부
static int   g_seeded       = 0;
// 마지막으로 시간 기반 업데이트가 적용된 시간
static time_t g_start_time   = 0;
// 누적 적용된 시간 단위 (몇 시간치 업데이트가 적용되었는지)
static int    g_applied_hours = 0;
// 현재 화면에 보이는 주식 개수
static int    g_visible_len[MAX_STOCKS];

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

/* 함수 목적: 주식을 보유한 사용자의 보유량을 CSV 파일에 저장한다.
 * 매개변수: user
 * 반환 값: 없음
 */
static void user_stock_save_holdings(User *user) {
    if (!user) return;

    char path[256];
    snprintf(path, sizeof(path), "data/stocks/%s.csv", user->name);

    FILE *fp = fopen(path, "w");
    if (!fp) {
        // 필요하면 디버그로그:
        // fprintf(stderr, "failed to open %s\n", path);
        return;
    }

    for (int i = 0; i < user->holding_count; ++i) {
        StockHolding *h = &user->holdings[i];
        if (h->qty <= 0) {
            continue; // 0 이하는 저장 안 함
        }
        fprintf(fp, "%s,%d\n", h->symbol, h->qty);
    }

    fclose(fp);
}

/* 함수 목적: 문자열 앞뒤의 공백을 제거한다.
 * 매개변수: str
 * 반환 값: 없음
 */
void trim_whitespace(char *str) {
    char *end;

    // 앞쪽 공백 제거
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) {
        // 문자열이 모두 공백인 경우
        *str = '\0';
        return;
    }

    // 뒤쪽 공백 제거
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    // 널문자 추가
    *(end + 1) = '\0';
}

/* 함수 목적: 사용자가 특정 심볼의 주식을 보유하고 있는지 찾는다.
 * 매개변수: user, symbol
 * 반환 값: 주식 보유량
 */
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

/* 함수 목적: 사용자가 특정 심볼의 주식을 보유하고 있으면 반환하고,
 *           없으면 새로 생성하여 반환한다.
 * 매개변수: user, symbol
 * 반환 값: 주식 보유량
 */
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

/* 함수 목적: 주식 거래를 시행한다.
 * 매개변수: username, symbol, qty, is_buy
 * 반환 값: 성공 여부
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

    /* 🔹 거래 성공했으니까 CSV에 현재 보유량 덤프 */
    user_stock_save_holdings(user);

    return 1;
}



/* 함수 목적: 주식 정보를 csv 파일에서 불러온다.
 * 매개변수: path
 * 반환 값: 없음
 */
static void stock_load_from_csv(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return;
    }

    char line[512];

    /* 1줄째: 시간일 수도 있고 아닐 수도 있음 */
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return;
    }

    /* 줄 끝의 개행 제거 */
    size_t len_line = strlen(line);
    while (len_line > 0 &&
           (line[len_line - 1] == '\n' || line[len_line - 1] == '\r')) {
        line[--len_line] = '\0';
    }

    /* 앞쪽 공백 스킵 */
    char *p = line;
    while (*p == ' ' || *p == '\t') {
        ++p;
    }

    /* ---------- 1단계: YYYYMMDDHHMMSS (14자리) 포맷 시도 ---------- */
    char digits[32] = {0};
    if (sscanf(p, "%14[0-9]", digits) == 1 && strlen(digits) == 14) {
        int year, mon, day, hour, min, sec;

        char buf_year[5] = {0};
        char buf_mon [3] = {0};
        char buf_day [3] = {0};
        char buf_hour[3] = {0};
        char buf_min [3] = {0};
        char buf_sec [3] = {0};

        memcpy(buf_year, digits + 0, 4);  // YYYY
        memcpy(buf_mon,  digits + 4, 2);  // MM
        memcpy(buf_day,  digits + 6, 2);  // DD
        memcpy(buf_hour, digits + 8, 2);  // HH
        memcpy(buf_min,  digits +10, 2);  // MM
        memcpy(buf_sec,  digits +12, 2);  // SS

        year = atoi(buf_year);
        mon  = atoi(buf_mon);
        day  = atoi(buf_day);
        hour = atoi(buf_hour);
        min  = atoi(buf_min);
        sec  = atoi(buf_sec);

        struct tm t;
        memset(&t, 0, sizeof(t));
        t.tm_year = year - 1900;   // 1900 기준
        t.tm_mon  = mon  - 1;      // 0~11
        t.tm_mday = day;
        t.tm_hour = hour;
        t.tm_min  = min;
        t.tm_sec  = sec;

        time_t ts = mktime(&t);    // 로컬타임(KST) 기준
        if (ts != (time_t)-1) {
            g_start_time = ts;
        }
    }

    memset(g_stocks, 0, sizeof(g_stocks));
    g_stock_count = 0;

    /* 실제 종목 라인들 파싱 */
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '\n' || line[0] == '\r' || line[0] == '#') {
            continue;
        }

        if (g_stock_count >= MAX_STOCKS) {
            break;
        }

        /* 개행 제거 */
        size_t l = strlen(line);
        while (l > 0 && (line[l - 1] == '\n' || line[l - 1] == '\r')) {
            line[--l] = '\0';
        }

        /* 형식: name,news,price1,price2,... */

        char *name = strtok(line, ",");
        if (!name) continue;

        char *news = strtok(NULL, ",");  // 뉴스 문자열 (쉼표 기준)

        /* 공백 정리 (trim_whitespace는 너가 이미 쓰던 함수 재사용) */
        trim_whitespace(name);
        if (news) {
            trim_whitespace(news);
        } else {
            news = "";
        }

        Stock *s = &g_stocks[g_stock_count];
        memset(s, 0, sizeof(*s));

        snprintf(s->name, sizeof(s->name), "%s", name);
        s->id = g_stock_count + 1;

        if (news && news[0] != '\0') {
            snprintf(s->news, sizeof(s->news), "%s", news);
        } else {
            snprintf(s->news, sizeof(s->news), "");  // 없으면 빈 문자열
        }

        /* 나머지 토큰들은 전부 가격 */
        int idx = 0;
        char *token = NULL;
        while ((token = strtok(NULL, ",")) != NULL) {
            if (idx >= 200) break;

            trim_whitespace(token);
            if (token[0] == '\0') continue;  // 빈 값 스킵

            s->log[idx++] = atoi(token);
        }

        if (idx == 0) {
            /* 가격 기록이 없으면 이 종목은 무시 */
            continue;
        }

        s->log_len    = idx;          // 전체 시계열 길이
        s->base_price = s->log[0];

        /* 시간 0 기준: 첫 번째 값이 현재가 */
        s->current_price  = s->log[0];
        s->previous_price = s->log[0];

        /* 이 종목은 지금 1개까지만 공개된 상태 */
        g_visible_len[g_stock_count] = 1;

        g_stock_count++;
    }

    fclose(fp);

    if (g_stock_count == 0) {
        /* 필요하면 여기서 디버그 로그 */
    }
}


/* 함수 목적: 걸정을 초기화한다.
 * 매개변수: 없음
 * 반환 값: 없음
 */
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

/* 함수 목적: 시간 경과에 따라 주식 정보를 업데이트한다.
 * 매개변수: 없음
 * 반환 값: 없음
 */
void stock_maybe_update_by_time(void) {
    ensure_seeded();

    time_t now = time(NULL);
    if (g_start_time == 0) {
        g_start_time = now;
    }

    double diff = difftime(now, g_start_time);
    if (diff < 0) diff = 0;

    /* 총 몇 시간이 지났는지 (1시간마다 한 칸씩) */
    int total_hours = (int)(diff / STOCK_STEP_SECONDS);

    if (total_hours <= g_applied_hours) {
        return;  // 새로 진행된 시간이 없음
    }

    int new_steps = total_hours - g_applied_hours;
    if (new_steps <= 0) {
        return;
    }

    g_applied_hours = total_hours;

    /* 새로 지난 시간만큼 한 칸씩 앞으로 진행 */
    for (int step = 0; step < new_steps; ++step) {
        for (int i = 0; i < g_stock_count; ++i) {
            Stock *s = &g_stocks[i];

            int visible = g_visible_len[i];
            int total   = s->log_len;

            /* 아직 더 보여줄 데이터가 있을 때만 한 칸 확장 */
            if (visible < total) {
                visible++;
                g_visible_len[i] = visible;

                s->previous_price = s->current_price;
                s->current_price  = s->log[visible - 1];
            }
            /* visible == total 이면 더 이상 늘리지 않고 마지막 값 유지 */
        }
    }
}

/* 함수 목적: 주식 심볼로 주식 정보를 찾는다.
 * 매개변수: symbol
 * 반환 값: 주식 포인터
 */
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

/* 함수 목적: 주식 정보를 특정 개수만큼 옮겨담는다.
 * 매개변수: out_arr, out_n
 * 반환 값: 성공 여부
 */
int stock_list(Stock *out_arr, int *out_n) {
    ensure_seeded();
    if (!out_arr || !out_n) return 0;

    for (int i = 0; i < g_stock_count; ++i) {
        Stock tmp = g_stocks[i];

        int visible = g_visible_len[i];
        if (visible <= 0) visible = 1;
        if (visible > tmp.log_len) visible = tmp.log_len;

        /* 이 시점에서 그래프/리스트가 보게 될 log 길이는 visible */
        tmp.log_len       = visible;
        tmp.current_price = tmp.log[visible - 1];
        tmp.previous_price= (visible >= 2)
                            ? tmp.log[visible - 2]
                            : tmp.current_price;

        out_arr[i] = tmp;
    }

    *out_n = g_stock_count;
    return 1;
}

/* 함수 목적: 주식의 히스토리를 out_buf에 복사한다. 
 * 매개변수: symbol, out_buf, max_len
 * 반환 값: 주식의 히스토리 길이
 */
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

/* data/stocks/(username).csv 에 저장된
 * "종목명,보유량" 들을 user->holdings[] 로 불러온다
 */
/* 함수 목적: 사용자의 주식 보유량을 CSV 파일에서 불러온다.
 * 매개변수: user
 * 반환 값: 없음
 */
static void user_stock_load_holdings(User *user) {
    if (!user) return;

    char path[256];
    snprintf(path, sizeof(path), "data/stocks/%s.csv", user->name);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        return;  // 파일 없으면 보유량 없음
    }

    user->holding_count = 0;  // 초기화

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        // 공백/개행 제거
        char *p = strtok(line, ", \t\r\n");
        if (!p) continue;

        char symbol[64];
        snprintf(symbol, sizeof(symbol), "%s", p);

        p = strtok(NULL, ", \t\r\n");
        if (!p) continue;

        int qty = atoi(p);
        if (qty <= 0) continue;

        if (user->holding_count >= MAX_HOLDINGS)
            break;

        StockHolding *h = &user->holdings[user->holding_count++];
        memset(h, 0, sizeof(*h));
        snprintf(h->symbol, sizeof(h->symbol), "%s", symbol);
        h->qty = qty;
    }

    fclose(fp);
}
