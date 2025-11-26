/*
 * 파일 목적: tui_stock 관련 터미널 UI 로직 구현
 * 작성자: 박성우
 */
#include "../../include/ui/tui_stock.h"

#include <stdlib.h>

#include "../../include/domain/stock.h"
#include "../../include/domain/user.h"
#include "../../include/ui/tui_common.h"
#include "../../include/ui/tui_ncurses.h"

/* 함수 목적: 주식 시장 UI 표시 및 거래 처리
 * 매개변수: user
 * 반환 값: 없음
 */
void tui_stock_show_market(User *user) {
    if (!user) {
        return;
    }
    Stock stocks[16];
    int count = 0;
    if (!stock_list(stocks, &count) || count == 0) {
        tui_ncurses_toast("No stocks available for trading", 800);
        return;
    }
    int height = LINES - 4;
    int width = COLS - 6;
    WINDOW *win = tui_common_create_box(height, width, 2, 3, "Stock Market (Enter buy / s sell / q close)");
    int highlight = 0;
    keypad(win, TRUE);
    while (1) {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 0, 2, " Stock Market - Deposit:%dCr Cash:%dCr ", user->bank.balance, user->bank.cash);
        int visible = height - 4;
        for (int i = 0; i < count && i < visible; ++i) {
            if (i == highlight) {
                wattron(win, A_REVERSE);
            }
            mvwprintw(win, 1 + i, 2, "%s Price:%4d News:%s", stocks[i].name, stocks[i].current_price, stocks[i].news);
            if (i == highlight) {
                wattroff(win, A_REVERSE);
            }
        }
        int row = height - 3;
        mvwprintw(win, row, 2, "Holdings:");
        for (int i = 0; i < user->holding_count && i < 3; ++i) {
            mvwprintw(win, row, 14 + i * 12, "%s x%d", user->holdings[i].symbol, user->holdings[i].qty);
        }
        wrefresh(win);
        int ch = wgetch(win);
        if (ch == KEY_UP) {
            highlight = (highlight - 1 + count) % count;
        } else if (ch == KEY_DOWN) {
            highlight = (highlight + 1) % count;
        } else if (ch == '\n' || ch == '\r') {
            if (stock_deal(user->name, stocks[highlight].name, 1, 1)) {
                tui_ncurses_toast("Buy complete", 700);
            } else {
                tui_ncurses_toast("Buy failed", 700);
            }
        } else if (ch == 's' || ch == 'S') {
            if (stock_deal(user->name, stocks[highlight].name, 1, 0)) {
                tui_ncurses_toast("Sell complete", 700);
            } else {
                tui_ncurses_toast("Sell failed", 700);
            }
        } else if (ch == 'q' || ch == 27) {
            break;
        }
    }
    tui_common_destroy_box(win);
}
