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
        tui_ncurses_toast("No ongoing auctions", 800);
        return;
    }
    int height = LINES - 4;
    int width = COLS - 6;
    WINDOW *win = tui_common_create_box(height, width, 2, 3, "Auction House (Enter buyout / b bid / q close)");
    int highlight = 0;
    keypad(win, TRUE);
    while (1) {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 0, 2, " Auction House - Deposit:%dCr Cash:%dCr ", user->bank.balance, user->bank.cash);
        for (int i = 0; i < count && i < height - 2; ++i) {
            if (i == highlight) {
                wattron(win, A_REVERSE);
            }
            mvwprintw(win, 1 + i, 2, "%s Buyout:%dCr Stock:%d", items[i].name, items[i].cost, items[i].stock);
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
                tui_ncurses_toast("Buyout complete", 800);
            } else {
                tui_ncurses_toast("Buyout failed", 800);
            }
        } else if (ch == 'b' || ch == 'B') {
            int bid_price = 0;
            if (tui_ncurses_prompt_number(win, "Bid price", &bid_price) && bid_price > 0) {
                if (auction_deal(user->name, &items[highlight], bid_price, 0)) {
                    tui_ncurses_toast("Bid placed", 800);
                } else {
                    tui_ncurses_toast("Bid failed", 800);
                }
            }
        } else if (ch == 'q' || ch == 27) {
            break;
        }
    }
    tui_common_destroy_box(win);
}
