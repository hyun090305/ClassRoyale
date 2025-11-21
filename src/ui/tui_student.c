#include "../../include/ui/tui_student.h"

#include <stdlib.h>
#include <string.h>

#include "../../include/domain/account.h"
#include "../../include/domain/admin.h"
#include "../../include/domain/auction.h"
#include "../../include/domain/economy.h"
#include "../../include/domain/mission.h"
#include "../../include/domain/shop.h"
#include "../../include/domain/stock.h"
#include "../../include/domain/user.h"
#include "../../include/ui/tui_auction.h"
#include "../../include/ui/tui_common.h"
#include "../../include/ui/tui_ncurses.h"
#include "../../include/ui/tui_stock.h"

static void ensure_student_seed(User *user) {
    if (!user || user->mission_count > 0) {
        return;
    }
    Mission open[8];
    int count = 0;
    if (mission_list_open(open, &count)) {
        for (int i = 0; i < count && i < 4; ++i) {
            admin_assign_mission(user->name, &open[i]);
        }
    }
}

static void render_mission_preview(WINDOW *win, const User *user) {
    mvwprintw(win, 1, 2, "Completed %d of %d missions", user->completed_missions, user->mission_count);
    int row = 2;
    for (int i = 0; i < user->mission_count && row < getmaxy(win) - 1; ++i, ++row) {
        const Mission *mission = &user->missions[i];
        mvwprintw(win, row, 2, "#%d %-12s [%s] +%dCr", mission->id, mission->name,
              mission->completed ? "Completed" : "In Progress", mission->reward);
    }
    if (user->mission_count == 0) {
        mvwprintw(win, row, 2, "No assigned missions.");
    }
    wrefresh(win);
}

static void render_shop_preview(WINDOW *win) {
    Shop shops[2];
    int count = 0;
    if (!shop_list(shops, &count) || count == 0) {
        mvwprintw(win, 1, 2, "No shop data");
        wrefresh(win);
        return;
    }
    const Shop *shop = &shops[0];
    mvwprintw(win, 1, 2, "%s - Today's Featured Items", shop->name);
    int limit = getmaxy(win) - 2;
    for (int i = 0; i < shop->item_count && i < limit; ++i) {
        mvwprintw(win, 2 + i, 2, "%s %3dCr [Stock:%2d]", shop->items[i].name, shop->items[i].cost,
              shop->items[i].stock);
    }
    wrefresh(win);
}

static void render_news(WINDOW *win) {
    const char *lines[] = {
        "Recent Notices",
        " - Mission #12 deadline D-1",
        " - QOTD reward +20Cr",
        " - Seat passes sold out"
    };
    tui_common_print_multiline(win, 1, 2, lines, sizeof(lines) / sizeof(lines[0]));
    mvwprintw(win, getmaxy(win) - 3, 2, "QOTD: How to save allowance?");
    mvwprintw(win, getmaxy(win) - 2, 4, "1) Goals  2) Immediate spending  3) Random investment");
    mvwprintw(win, getmaxy(win) - 1, 4, "[q] Respond on submission screen");
    wrefresh(win);
}

static void draw_dashboard(User *user, const char *status) {
    erase();
    mvprintw(1, (COLS - 30) / 2, "Class Royale - Student Dashboard");
    mvprintw(3, 2, "Name: %s | Balance: %d Cr | Rating: %c", user->name, user->bank.balance, user->bank.rating);
    mvprintw(4, 2, "Items owned: %d | Stocks owned: %d", 10, user->holding_count);
    int percent = user->total_missions > 0 ? (user->completed_missions * 100) / user->total_missions : 0;
    mvprintw(5, 2, "Mission Completion Rate:");
    tui_common_draw_progress(stdscr, 5, 18, COLS / 2, percent);

    int col_width = COLS / 2 - 3;
    int box_height = (LINES - 10) / 2;
    WINDOW *mission_win = tui_common_create_box(box_height, col_width, 7, 2, "Missions");
    render_mission_preview(mission_win, user);
    tui_common_destroy_box(mission_win);

    WINDOW *account_win = tui_common_create_box(box_height, col_width, 7 + box_height, 2, "Account Status");
    mvwprintw(account_win, 1, 2, "Deposit Balance: %d Cr", user->bank.balance);
    mvwprintw(account_win, 2, 2, "Estimated Tax: %d Cr", econ_tax(&user->bank));
    mvwprintw(account_win, 4, 2, "Recent Transactions");
    mvwprintw(account_win, 5, 4, "+40Cr Mission #12 reward");
    mvwprintw(account_win, 6, 4, "-80Cr Seat pass purchase");
    wrefresh(account_win);
    tui_common_destroy_box(account_win);

    WINDOW *shop_win = tui_common_create_box(box_height, col_width, 7, col_width + 4, "Shop/Marketplace");
    render_shop_preview(shop_win);
    tui_common_destroy_box(shop_win);

    WINDOW *news_win = tui_common_create_box(box_height, col_width, 7 + box_height, col_width + 4, "Notices & QOTD");
    render_news(news_win);
    tui_common_destroy_box(news_win);

    tui_common_draw_help("m:Missions s:Shop a:Account k:Stocks u:Auctions q:Logout");
    tui_ncurses_draw_status(status);
    refresh();
}

static void handle_mission_board(User *user) {
    if (!user) {
        return;
    }
    int height = LINES - 4;
    int width = COLS - 6;
    WINDOW *win = tui_common_create_box(height, width, 2, 3, "Mission Board (Enter to complete/ a new mission/ q to close)");
    int highlight = 0;
    keypad(win, TRUE);
    int running = 1;
    while (running) {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 0, 2, " Mission Board (Enter to complete/ a new mission/ q to close) ");
        int available = user->mission_count;
        if (available == 0) {
            mvwprintw(win, 2, 2, "No assigned missions. Press 'a' to accept a new mission!");
        }
        for (int i = 0; i < available && i < height - 2; ++i) {
            if (i == highlight) {
                wattron(win, A_REVERSE);
            }
            Mission *mission = &user->missions[i];
            mvwprintw(win, 1 + i, 2, "#%d %-20s [%s] +%d", mission->id, mission->name,
                      mission->completed ? "Completed" : "In Progress", mission->reward);
            if (i == highlight) {
                wattroff(win, A_REVERSE);
            }
        }
        wrefresh(win);
        int ch = wgetch(win);
        if ((ch == KEY_UP || ch == KEY_DOWN) && available > 0) {
            if (ch == KEY_UP) {
                highlight = (highlight - 1 + available) % available;
            } else {
                highlight = (highlight + 1) % available;
            }
        } else if ((ch == '\n' || ch == '\r') && available > 0) {
                if (mission_complete(user->name, user->missions[highlight].id)) {
                tui_ncurses_toast("Mission complete! Reward granted", 900);
            } else {
                tui_ncurses_toast("Failed to complete mission", 900);
            }
        } else if (ch == 'a' || ch == 'A') {
            Mission open[8];
            int count = 0;
            if (mission_list_open(open, &count) && count > 0) {
                admin_assign_mission(user->name, &open[rand() % count]);
                tui_ncurses_toast("A new mission has been added", 900);
            }
        } else if (ch == 'q' || ch == 27) {
            running = 0;
        }
    }
    tui_common_destroy_box(win);
}

static void handle_shop_view(User *user) {
    Shop shops[2];
    int count = 0;
    if (!shop_list(shops, &count) || count == 0) {
        tui_ncurses_toast("No shop information", 800);
        return;
    }
    Shop *shop = &shops[0];
    int height = LINES - 4;
    int width = COLS - 6;
    WINDOW *win = tui_common_create_box(height, width, 2, 3, "Shop (Enter to buy / s to sell / q to close)");
    int highlight = 0;
    keypad(win, TRUE);
    while (1) {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 0, 2, " %s Shop - Balance %dCr ", shop->name, user->bank.balance);
        int visible = height - 2;
        for (int i = 0; i < shop->item_count && i < visible; ++i) {
            if (i == highlight) {
                wattron(win, A_REVERSE);
            }
            mvwprintw(win, 1 + i, 2, "%s %3dCr Stock:%2d", shop->items[i].name, shop->items[i].cost, shop->items[i].stock);
            if (i == highlight) {
                wattroff(win, A_REVERSE);
            }
        }
        wrefresh(win);
        int ch = wgetch(win);
        if (shop->item_count == 0) {
            mvwprintw(win, 2, 2, "No items registered");
            wrefresh(win);
            int ch_wait = wgetch(win);
            if (ch_wait == 'q' || ch_wait == 27) {
                break;
            }
            continue;
        }
        if (ch == KEY_UP) {
            highlight = (highlight - 1 + shop->item_count) % shop->item_count;
        } else if (ch == KEY_DOWN) {
            highlight = (highlight + 1) % shop->item_count;
        } else if (ch == '\n' || ch == '\r') {
            if (shop_buy(user->name, &shop->items[highlight], 1)) {
                tui_ncurses_toast("Purchase complete", 800);
            } else {
                tui_ncurses_toast("Purchase failed - check balance/stock", 800);
            }
        } else if (ch == 's' || ch == 'S') {
            if (shop_sell(user->name, &shop->items[highlight], 1)) {
                tui_ncurses_toast("Sale complete", 800);
            } else {
                tui_ncurses_toast("Sale failed", 800);
            }
        } else if (ch == 'q' || ch == 27) {
            break;
        }
    }
    tui_common_destroy_box(win);
}

static void handle_account_view(User *user) {
    int height = 12;
    int width = 60;
    WINDOW *win = tui_common_create_box(height, width, (LINES - height) / 2, (COLS - width) / 2,
                                        "Account Management (d deposit / b borrow / r repay / q close)");
    int running = 1;
    while (running) {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 0, 2, " Account Management ");
        mvwprintw(win, 1, 2, "Deposit: %d Cr", user->bank.balance);
        mvwprintw(win, 2, 2, "Rating: %c", user->bank.rating);
        mvwprintw(win, 3, 2, "Estimated Tax: %d Cr", econ_tax(&user->bank));
        mvwprintw(win, 5, 2, "Commands: d)deposit  b)borrow  r)repay  q)close");
        wrefresh(win);
        int ch = wgetch(win);
        if (ch == 'd' || ch == 'b' || ch == 'r') {
            char label[32];
                if (ch == 'd') {
                snprintf(label, sizeof(label), "Deposit amount");
            } else if (ch == 'b') {
                snprintf(label, sizeof(label), "Borrow amount");
            } else {
                snprintf(label, sizeof(label), "Repay amount");
            }
            int amount = 0;
            if (tui_ncurses_prompt_number(win, label, &amount) && amount > 0) {
                int ok = 0;
                if (ch == 'd') {
                    ok = econ_deposit(&user->bank, amount);
                } else if (ch == 'b') {
                    ok = econ_borrow(&user->bank, amount);
                } else {
                    ok = econ_repay(&user->bank, amount);
                }
                tui_ncurses_toast(ok ? "Processed" : "Transaction failed", 800);
            }
        } else if (ch == 'q' || ch == 27) {
            running = 0;
        }
    }
    tui_common_destroy_box(win);
}

void tui_student_loop(User *user) {
    if (!user) {
        return;
    }
    ensure_student_seed(user);
    const char *status = "m:Missions s:Shop a:Account k:Stocks u:Auctions q:Logout";
    int running = 1;
    while (running) {
        draw_dashboard(user, status);
        int ch = getch();
        switch (ch) {
            case 'm':
            case 'M':
                handle_mission_board(user);
                status = "Complete with Enter in missions screen";
                break;
            case 's':
            case 'S':
                handle_shop_view(user);
                status = "Purchased item in shop with Enter";
                break;
            case 'a':
            case 'A':
                handle_account_view(user);
                status = "Account operation completed";
                break;
            case 'k':
            case 'K':
                tui_stock_show_market(user);
                status = "Completed stock trade";
                break;
            case 'u':
            case 'U':
                tui_auction_show_house(user);
                status = "Active in auction house";
                break;
            case 'q':
            case 'Q':
                running = 0;
                break;
            default:
                status = "Check keyboard shortcuts";
                break;
        }
    }
}
