#include "../../include/ui/tui_auction.h"

#include "../../include/domain/auction.h"
#include "../../include/ui/tui_common.h"
#include "../../include/ui/tui_ncurses.h"

void tui_auction_show_house(User *user) {
    if (!user) {
        return;
    }
    Item items[16];
    int count = 0;
    if (!auction_list(items, &count) || count == 0) {
        tui_ncurses_toast("진행 중인 경매가 없습니다", 800);
        return;
    }
    int height = LINES - 4;
    int width = COLS - 6;
    WINDOW *win = tui_common_create_box(height, width, 2, 3, "경매장 (Enter 즉시구매 / b 입찰 / q 닫기)");
    int highlight = 0;
    keypad(win, TRUE);
    while (1) {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 0, 2, " 경매장 - 보유 잔액 %dCr ", user->bank.balance);
        for (int i = 0; i < count && i < height - 2; ++i) {
            if (i == highlight) {
                wattron(win, A_REVERSE);
            }
            mvwprintw(win, 1 + i, 2, "%s 즉시구매:%dCr 재고:%d", items[i].name, items[i].cost, items[i].stock);
            if (i == highlight) {
                wattroff(win, A_REVERSE);
            }
        }
        wrefresh(win);
        int ch = wgetch(win);
        if (ch == KEY_UP) {
            highlight = (highlight - 1 + count) % count;
        } else if (ch == KEY_DOWN) {
            highlight = (highlight + 1) % count;
        } else if (ch == '\n' || ch == '\r') {
            if (auction_deal(user->name, &items[highlight], items[highlight].cost, 1)) {
                tui_ncurses_toast("즉시 구매 완료", 800);
            } else {
                tui_ncurses_toast("즉시 구매 실패", 800);
            }
        } else if (ch == 'b' || ch == 'B') {
            int bid_price = 0;
            if (tui_ncurses_prompt_number(win, "입찰가", &bid_price) && bid_price > 0) {
                if (auction_deal(user->name, &items[highlight], bid_price, 0)) {
                    tui_ncurses_toast("입찰되었습니다", 800);
                } else {
                    tui_ncurses_toast("입찰 실패", 800);
                }
            }
        } else if (ch == 'q' || ch == 27) {
            break;
        }
    }
    tui_common_destroy_box(win);
}
