#include "../../include/ui/tui_stock.h"

#include <stdlib.h>

#include "../../include/domain/stock.h"
#include "../../include/domain/user.h"
#include "../../include/ui/tui_common.h"
#include "../../include/ui/tui_ncurses.h"

void tui_stock_show_market(User *user) {
    if (!user) {
        return;
    }
    Stock stocks[16];
    int count = 0;
    if (!stock_list(stocks, &count) || count == 0) {
        tui_ncurses_toast("거래 가능한 주식이 없습니다", 800);
        return;
    }
    int height = LINES - 4;
    int width = COLS - 6;
    WINDOW *win = tui_common_create_box(height, width, 2, 3, "주식 시장 (Enter 매수 / s 매도 / q 닫기)");
    int highlight = 0;
    keypad(win, TRUE);
    while (1) {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 0, 2, " 주식 시장 - 보유 잔액 %dCr ", user->bank.balance);
        int visible = height - 4;
        for (int i = 0; i < count && i < visible; ++i) {
            if (i == highlight) {
                wattron(win, A_REVERSE);
            }
            mvwprintw(win, 1 + i, 2, "%s 가격:%4d 뉴스:%s", stocks[i].name, stocks[i].current_price, stocks[i].news);
            if (i == highlight) {
                wattroff(win, A_REVERSE);
            }
        }
        int row = height - 3;
        mvwprintw(win, row, 2, "보유 종목:");
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
                tui_ncurses_toast("매수 완료", 700);
            } else {
                tui_ncurses_toast("매수 실패", 700);
            }
        } else if (ch == 's' || ch == 'S') {
            if (stock_deal(user->name, stocks[highlight].name, 1, 0)) {
                tui_ncurses_toast("매도 완료", 700);
            } else {
                tui_ncurses_toast("매도 실패", 700);
            }
        } else if (ch == 'q' || ch == 27) {
            break;
        }
    }
    tui_common_destroy_box(win);
}
