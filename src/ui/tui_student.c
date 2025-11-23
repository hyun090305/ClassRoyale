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
#include "../../include/core/csv.h"
#include "../../include/domain/notification.h"
#include "../../include/domain/account.h"

static void ensure_student_seed(User *user) {
    if (!user || user->mission_count > 0) {
        return;
    }
    /* try to load persisted missions from CSV first */
    mission_load_user(user->name, user);
    Mission open[8];
    int count = 0;
    if (mission_list_open(open, &count)) {
        for (int i = 0; i < count && i < 4; ++i) {
            admin_assign_mission(user->name, &open[i]);
        }
    }
    /* ensure per-user tx file exists and set bank metadata */
    csv_ensure_dir("data/txs");
    snprintf(user->bank.name, sizeof(user->bank.name), "%s", user->name);
    char path[512];
    snprintf(path, sizeof(path), "data/txs/%s.csv", user->name);
    if (!user->bank.fp) {
        user->bank.fp = fopen(path, "a+");
    }
}

// --- QOTD viewer integration ---
static char *qotd_solved_users[256];
static int qotd_solved_count = 0;

static int qotd_is_solved(const char *name) {
    if (!name) return 0;
    for (int i = 0; i < qotd_solved_count; ++i) {
        if (qotd_solved_users[i] && strcmp(qotd_solved_users[i], name) == 0) return 1;
    }
    return 0;
}

static void qotd_mark_solved(const char *name) {
    if (!name || qotd_solved_count >= (int)(sizeof(qotd_solved_users)/sizeof(qotd_solved_users[0]))) return;
    for (int i = 0; i < qotd_solved_count; ++i) {
        if (qotd_solved_users[i] && strcmp(qotd_solved_users[i], name) == 0) return;
    }
    qotd_solved_users[qotd_solved_count++] = strdup(name);
}

/* QOTD viewer:
 * - open with 'd' from student menu
 * - shows question and choices
 * - enter number to answer
 * - correct => award reward, mark solved, cannot reopen
 * - wrong => show "Try again" at bottom and reduce reward by 5Cr
 * - press 'q' to exit viewer
 */
static void handle_qotd_view(User *user) {
    if (!user) return;
    if (qotd_is_solved(user->name)) {
        tui_ncurses_toast("QOTD already solved", 900);
        return;
    }

    const char *question = "QOTD: How to save allowance?";
    const char *opts[] = {
        "1) Goals",
        "2) Immediate spending",
        "3) Random investment"
    };
    const int correct_choice = 1; /* 1-based index of correct option */
    int reward = 20;

    int height = 10;
    int width = 60;
    int starty = (LINES - height) / 2;
    int startx = (COLS - width) / 2;
    WINDOW *win = tui_common_create_box(height, width, starty, startx, "Question of the Day (press q to close)");
    keypad(win, TRUE);

    int running = 1;
    while (running) {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 1, 2, "%s", question);
        for (int i = 0; i < (int)(sizeof(opts)/sizeof(opts[0])); ++i) {
            mvwprintw(win, 3 + i, 4, "%s", opts[i]);
        }
        mvwprintw(win, height - 4, 2, "Current reward: %d Cr", reward);
        mvwprintw(win, height - 3, 2, "Enter option number to answer, q to quit");
        mvwprintw(win, height - 2, 2, ""); /* reserved for messages (Try again etc) */
        wrefresh(win);

        int ch = wgetch(win);
        if (ch == 'q' || ch == 27) {
            break;
        }
        if (ch >= '1' && ch <= '9') {
            int sel = ch - '0';
            if (sel == correct_choice) {
                user->bank.balance += reward;
                qotd_mark_solved(user->name);
                mvwprintw(win, height - 2, 2, "Correct! +%dCr awarded. Press any key.", reward);
                wrefresh(win);
                wgetch(win);
                tui_ncurses_toast("Correct! Reward granted", 1000);
                running = 0;
                break;
            } else {
                reward -= 5;
                if (reward < 0) reward = 0;
                mvwprintw(win, height - 2, 2, "Try again - reward now %d Cr   ", reward);
                wrefresh(win);
                /* keep the message visible a bit longer inside the QOTD window */
                napms(1200); /* 1200ms pause */
                 /* continue loop so user can try again or press q */
            }
        }
    }

    tui_common_destroy_box(win);
}
// --- end QOTD integration ---

static void render_mission_preview(WINDOW *win, const User *user) {
    mvwprintw(win, 1, 2, "Completed %d of %d missions", user->completed_missions, user->mission_count);
    int row = 2;
    for (int i = 0; i < user->mission_count && row < getmaxy(win) - 1; ++i, ++row) {
        const Mission *mission = &user->missions[i];
        mvwprintw(win, row, 2, "#%d %-12s [%s] +%dCr", mission->id, mission->name,
              mission->completed ? "Completed" : "In Progress", mission->reward);
    }
        /* show QOTD hint only if the current user hasn't solved it yet */
    if (user && !qotd_is_solved(user->name)) {
        mvwprintw(win, getmaxy(win) - 4, 2, "QOTD: How to save allowance?");
        mvwprintw(win, getmaxy(win) - 3, 4, "1) Goals  2) Immediate spending  3) Random investment");
        mvwprintw(win, getmaxy(win) - 2, 4, "[d] Respond on submission screen");
    }

    if (user->mission_count == 0 && qotd_is_solved(user->name)) {
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

/* forward declare QOTD helper so render_news can check per-user state */
static int qotd_is_solved(const char *name);

static void render_news(WINDOW *win, const User *user) {
    const char *lines[] = {
        "Recent Notices",
        " - Mission #12 deadline D-1",
        " - Seat passes sold out"
    };
    tui_common_print_multiline(win, 1, 2, lines, sizeof(lines) / sizeof(lines[0]));
    wrefresh(win);
}

static void draw_dashboard(User *user, const char *status) {
    erase();
    mvprintw(1, (COLS - 30) / 2, "Class Royale - Student Dashboard");
    mvprintw(3, 2, "Name: %s | Balance: %d Cr | Rating: %c", user->name, user->bank.balance, user->bank.rating);
    mvprintw(4, 2, "Items owned: %d | Stocks owned: %d", 10, user->holding_count);
    int percent = user->total_missions > 0 ? (user->completed_missions * 100) / user->total_missions : 0;
    const char *mc_label = "Mission Completion Rate:";
    int label_x = 2;
    mvprintw(5, label_x, "%s", mc_label);
    int progress_x = label_x + (int)strlen(mc_label) + 1; /* place progress after label + 1 space */
    int progress_width = COLS - progress_x - 2; /* leave right margin */
    if (progress_width <= 0) progress_width = 10; /* fallback */
    if (progress_width > COLS / 2) progress_width = COLS / 2; /* sensible cap */
    tui_common_draw_progress(stdscr, 5, progress_x, progress_width, percent);

    int col_width = COLS / 2 - 3;
    int box_height = (LINES - 10) / 2;
    WINDOW *mission_win = tui_common_create_box(box_height, col_width, 7, 2, "Missions[m]");
    render_mission_preview(mission_win, user);
    tui_common_destroy_box(mission_win);

    WINDOW *account_win = tui_common_create_box(box_height, col_width, 7 + box_height, 2, "Account Status");
    mvwprintw(account_win, 1, 2, "Deposit Balance: %d Cr", user->bank.balance);
    mvwprintw(account_win, 2, 2, "Estimated Tax: %d Cr", econ_tax(&user->bank));
    mvwprintw(account_win, 4, 2, "Recent Transactions");
    char txbuf[2048];
    int got = account_recent_tx(user->name, 6, txbuf, sizeof(txbuf));
    if (got > 0) {
        int row = 5;
        char *p = txbuf;
        while (p && *p && row < getmaxy(account_win)-1) {
            char *nl = strchr(p, '\n');
            if (nl) *nl = '\0';
            mvwprintw(account_win, row++, 4, "%s", p);
            if (!nl) break;
            p = nl + 1;
        }
    } else {
        mvwprintw(account_win, 5, 4, "No recent transactions");
    }
    wrefresh(account_win);
    tui_common_destroy_box(account_win);

    WINDOW *shop_win = tui_common_create_box(box_height, col_width, 7, col_width + 4, "Shop/Marketplace[s]");
    render_shop_preview(shop_win);
    tui_common_destroy_box(shop_win);

    WINDOW *news_win = tui_common_create_box(box_height, col_width, 7 + box_height, col_width + 4, "Notices");
    render_news(news_win, user);
    tui_common_destroy_box(news_win);

    tui_common_draw_help("m:Missions s:Shop a:Account d:QOTD q:Logout");
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
        mvwprintw(win, 0, 2, " Mission Board (Enter to complete/ q to close) ");
        int available = user->mission_count;
        if (available == 0) {
            mvwprintw(win, 2, 2, "No assigned missions.");
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
    int height = LINES - 4;
    int width = COLS - 6;
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

static void handle_notice_view(User *user) {
    int height = LINES-4;
    int width = COLS-6;
    WINDOW *win = tui_common_create_box(height, width, (LINES - height) / 2, (COLS - width) / 2,
                                        "Notices");
    int running = 1;
    keypad(win, TRUE);
    while (running) {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 1, 2, "Notices for %s", user->name);
        char buf[4096];
        int n = notify_recent_to_buf(user->name, 50, buf, sizeof(buf));
        if (n > 0) {
            int row = 3;
            char *p = buf;
            while (p && *p && row < getmaxy(win)-2) {
                char *nl = strchr(p, '\n');
                if (nl) *nl = '\0';
                mvwprintw(win, row++, 2, "%s", p);
                if (!nl) break;
                p = nl + 1;
            }
        } else {
            mvwprintw(win, 3, 2, "No notices.");
        }
        mvwprintw(win, getmaxy(win)-2, 2, "Press q to close");
        wrefresh(win);
        int ch = wgetch(win);
        if (ch == 'q' || ch == 'Q' || ch == 27) running = 0;
    }
    tui_common_destroy_box(win);
}

void tui_student_loop(User *user) {
    if (!user) {
        return;
    }
    ensure_student_seed(user);
    const char *status = "Shortcut Keys";
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
            case 'd':
            case 'D':
                handle_qotd_view(user);
                status = "QOTD";
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
            case 'n':
            case 'N':
                handle_notice_view(user);
                status = "";
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
