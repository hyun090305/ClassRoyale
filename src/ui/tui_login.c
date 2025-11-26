/*
 * 파일 목적: tui_login 관련 터미널 UI 로직 구현
 * 작성자: 채연우
 */
#include "../../include/ui/tui_login.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "../../include/domain/user.h"
#include "../../include/domain/economy.h"
#include "../../include/ui/tui_common.h"
#include "../../include/ui/tui_ncurses.h"

static void user_stock_load_holdings(User *user);
/* 함수 목적: 로그인 화면에서 환영 메시지와 메뉴를 그리는 함수
 * 매개변수: highlight, status_line
 * 반환 값: 없음
 */
static void draw_welcome(int highlight, const char *status_line) {
    static WINDOW *win = NULL;
    int width = 64;
    int height = 20;
    int starty = (LINES - height) / 2;
    int startx = (COLS - width) / 2;
    /* Ensure the standard screen is cleared so previous overlays don't persist
     * (calling refresh() after wrefresh(win) can redraw stdscr over the
     * window; clear stdscr before drawing and avoid a global refresh here). */
    clear();
    /* leave stdscr refreshed to reflect the cleared background */
    refresh();
    if (!win) {
        win = tui_common_create_box(height, width, starty, startx, "Class Royale");
    } else {
        werase(win);
        box(win, 0, 0);
        wattron(win, A_BOLD);
        mvwprintw(win, 0, 2, " %s ", "Class Royale");
        wattroff(win, A_BOLD);
    }

    tui_ncurses_draw_logo(win, 2, 4);
    const char *menu[] = {"Login", "Register", "Exit"};
    tui_common_draw_menu(win, menu, 3, highlight);
    mvwprintw(win, height - 3, 2, "Use arrow keys, Enter to select");
    if (status_line && *status_line) {
        mvwprintw(win, height - 4, 2, "%s", status_line);
    }
    wrefresh(win);
}

/* 함수 목적: 선생이나 학생을 결정하는 함수
 * 매개변수: form
 * 반환 값: 직급을 반환
 */
static RankEnum prompt_role(WINDOW *form) {
    const char *roles[] = {"Student", "Teacher"};
    int highlight = 0;
    while (1) {
        for (int i = 0; i < 2; ++i) {
            if (i == highlight) {
                wattron(form, A_REVERSE);
            }
            mvwprintw(form, 6 + i, 2, "(%c) %s", i == highlight ? '>' : ' ', roles[i]);
            if (i == highlight) {
                wattroff(form, A_REVERSE);
            }
        }
        mvwprintw(form, 5, 2, "Select role (arrow keys, Enter)");
        wrefresh(form);
        int ch = wgetch(form);
        if (ch == KEY_UP || ch == KEY_LEFT) {
            highlight = (highlight + 1) % 2;
        } else if (ch == KEY_DOWN || ch == KEY_RIGHT) {
            highlight = (highlight + 1) % 2;
        } else if (ch == '\n' || ch == '\r') {
            return highlight == 0 ? STUDENT : TEACHER;
        } else if (ch == 27) {
            return STUDENT;
        }
    }
}

/* 함수 목적: 문자열의 앞뒤 공백을 제거하는 함수
 * 매개변수: s
 * 반환 값: 없음
 */
static void trim_whitespace(char *s) {
    if (!s) return;
    // trim leading
    while (*s && isspace((unsigned char)*s)) {
        size_t len = strlen(s);
        memmove(s, s+1, len); /* include null terminator shift */
    }
    if (*s == '\0') return;
    // trim trailing
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) {
        *end = '\0';
        --end;
    }
}

enum {
    NAME_FIELD_CAP = sizeof(((User *)0)->name),
    PW_FIELD_CAP = sizeof(((User *)0)->pw)
};

/* 함수 목적: 로그인을 진행하는 함수
 * 매개변수: 없음
 * 반환 값: User 포인터를 반환
 */
static User *prompt_login(void) {
    int width = 60;
    int height = 12;
    WINDOW *form = tui_common_create_box(height, width, (LINES - height) / 2, (COLS - width) / 2, "Login");
    keypad(form,TRUE);
    char username[NAME_FIELD_CAP];
    char password[PW_FIELD_CAP];
    memset(username, 0, sizeof(username));
    memset(password, 0, sizeof(password));
    mvwprintw(form, 2, 2, "Please enter name and password");
    wrefresh(form);
    if (!tui_ncurses_prompt_line(form, 4, 2, "Name", username, sizeof(username), 0)) {
        tui_common_destroy_box(form);
        return NULL;
    }
    if (!tui_ncurses_prompt_line(form, 5, 2, "Password", password, sizeof(password), 1)) {
        tui_common_destroy_box(form);
        return NULL;
    }
    if (!user_auth(username, password)) {
        tui_ncurses_toast("Login failed - check credentials", 1000);
        tui_common_destroy_box(form);
        return NULL;
    }
    User *user = user_lookup(username);

    if (user) {
    user_stock_load_holdings(user);
}
    /* Apply accumulated hourly interest since last_interest_ts */
    if (user) {
        long now = (long)time(NULL);
        if (user->bank.last_interest_ts == 0) {
            user->bank.last_interest_ts = now; /* initialize without applying retroactive interest */
        } else {
            long diff = now - user->bank.last_interest_ts;
            int hours = (int)(diff / 3600);
            if (hours > 0) {
                econ_apply_hourly_interest(user, hours);
                user->bank.last_interest_ts = now;
                /* persist updated balance/interest timestamp */
                user_update_balance(user->name, user->bank.balance);
            }
        }
    }
    tui_common_destroy_box(form);
    return user;
}

/* 함수 목적: 회원가입을 진행하는 함수
 * 매개변수: 없음
 * 반환 값: 없음
 */
static void prompt_register(void) {
    int width = 60;
    int height = 14;
    WINDOW *form = tui_common_create_box(height, width, (LINES - height) / 2, (COLS - width) / 2, "Register");
    char username[NAME_FIELD_CAP];
    char password[PW_FIELD_CAP];
    memset(username, 0, sizeof(username));
    memset(password, 0, sizeof(password));
    mvwprintw(form, 2, 2, "Create a new account");
    wrefresh(form);
    if (!tui_ncurses_prompt_line(form, 3, 2, "Name", username, sizeof(username), 0)) {
        tui_common_destroy_box(form);
        return;
    }
    if (!tui_ncurses_prompt_line(form, 4, 2, "Password", password, sizeof(password), 1)) {
        tui_common_destroy_box(form);
        return;
    }
    /* trim whitespace and validate non-empty inputs */
    trim_whitespace(username);
    trim_whitespace(password);
    if (username[0] == '\0' || password[0] == '\0') {
        tui_ncurses_toast("Name and password cannot be empty", 1200);
        tui_common_destroy_box(form);
        return;
    }
    RankEnum role = prompt_role(form);
    User newbie = {0};
    snprintf(newbie.name, sizeof(newbie.name), "%s", username);
    snprintf(newbie.id, sizeof(newbie.id), "%s", username);
    snprintf(newbie.pw, sizeof(newbie.pw), "%s", password);
    newbie.isadmin = role;
    snprintf(newbie.bank.name, sizeof(newbie.bank.name), "%s", username);
    newbie.bank.balance = role == STUDENT ? 1000 : 5000;
    newbie.bank.cash = 0;
    newbie.bank.loan = 0;
    /* rating removed */
    if (user_register(&newbie)) {
        tui_ncurses_toast("Registration complete! Please log in", 1200);
        /* persist only on successful registration */
        FILE *fp = fopen("data/users.csv", "a");
        if (fp) {
            fprintf(fp, "\n%s,%s,%d", username, password, (int)role);
            fclose(fp);
        }
        FILE *fp1 = fopen("data/accounts.csv", "a");
        if (fp1) {
            /* persist format: name,balance,cash,loan,last_interest_ts,log */
            long ts = (long)time(NULL);
            fprintf(fp1, "\n%s,%d,%d,%d,%ld,", username, newbie.bank.balance, newbie.bank.cash, newbie.bank.loan, ts);
            fclose(fp1);
        }

        {
            char path[256];
            snprintf(path, sizeof(path), "data/stocks/%s.csv", username);
            FILE *fp_stocks = fopen(path, "w");
            if (fp_stocks) {
                // 일단 빈 파일로 생성만 해둠
                fclose(fp_stocks);
            }
        }
    
    } else {
        tui_ncurses_toast("Registration failed - name may be duplicate", 1200);
    }
    tui_common_destroy_box(form);
    draw_welcome(1, "After registration, please log in");
}
User *tui_login_flow(void) {
    int selection = 0;
    const int menu_count = 3;
    const char *status = "";
    while (1) {
        draw_welcome(selection, status);
        int ch = getch();
        if (ch == KEY_UP) {
            selection = (selection - 1 + menu_count) % menu_count;
        } else if (ch == KEY_DOWN) {
            selection = (selection + 1) % menu_count;
        } else if (ch == '\n' || ch == '\r') {
            if (selection == 0) {
                User *user = prompt_login();
                if (user) {
                    return user;
                }
                status = "Login failed";
            } else if (selection == 1) {
                prompt_register();
                status = "After registration, please log in";
            } else {
                return NULL;
            }
        } else if (ch == 'q' || ch == 'Q') {
            return NULL;
        }
    }
}

/* data/stocks/(username).csv 에 저장된
 * "종목명,보유량" 들을 user->holdings[] 로 불러온다
 */
/* 함수 목적: 사용자의 주식 정보를 불러오는 함수
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
