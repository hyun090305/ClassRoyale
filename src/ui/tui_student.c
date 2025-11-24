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
#include "../../include/domain/message.h"

static int user_has_mission(User *user, int mission_id) {
    if (!user) return 0;
    for (int i = 0; i < user->mission_count; ++i) {
        if (user->missions[i].id == mission_id) return 1;
    }
    return 0;
}

static void ensure_student_seed(User *user) {
    if (!user || user->mission_count > 0) {
        return;
    }
    /* reload mission catalog from disk at each login/start of student UI */
    mission_refresh_catalog();

    /* reload mission catalog from disk at each login/start of student UI */
    mission_refresh_catalog();

    /* try to load persisted missions from CSV first */
    mission_load_user(user->name, user);
    Mission open[8];
    int count = 0;
    if (mission_list_open(open, &count)) {
        for (int i = 0; i < count && i < 4; ++i) {
            /* avoid assigning a mission the user already has (prevents duplicates) */
            if (!user_has_mission(user, open[i].id)) {
                admin_assign_mission(user->name, &open[i]);
            }
        }
    }
    /* ensure per-user tx file exists and set bank metadata */
    snprintf(user->bank.name, sizeof(user->bank.name), "%s", user->name);
    // 로그 버퍼가 비어 있으면 초기 메시지 넣기 (원하면 생략해도 됨)
    if (user->bank.log[0] == '\0') {
        snprintf(
            user->bank.log,
            sizeof(user->bank.log),
            "== Transaction log for %s ==\n",
            user->name);
    }
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
                /* use account_add_tx to adjust balance and persist tx */
                int ok = account_add_tx(user, reward, "QOTD");
                if (ok) {
                    qotd_mark_solved(user->name);
                    /* persist balance to accounts CSV as other flows do */
                    user_update_balance(user->name, user->bank.balance);
                    mvwprintw(win, height - 2, 2, "Correct! +%dCr awarded. Press any key.", reward);
                    wrefresh(win);
                    wgetch(win);
                    tui_ncurses_toast("Correct! Reward granted", 1000);
                    running = 0;
                    break;
                } else {
                    mvwprintw(win, height - 2, 2, "Transaction failed. Try again later.");
                    wrefresh(win);
                    napms(1000);
                    break;
                }
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
    if (!win) return;
    int inner_rows = getmaxy(win) - 2;
    if (inner_rows <= 0) return;
    if (!user) {
        mvwprintw(win, 1, 2, "No notices.");
        wrefresh(win);
        return;
    }

    int row = 1;

    /* Notifications section */
    mvwprintw(win, row++, 2, "Notices:");
    char notice_buf[1024];
    notice_buf[0] = '\0';
    int notice_len = notify_recent_to_buf(user->name, inner_rows / 2 + 2, notice_buf, sizeof(notice_buf));
    if (notice_len > 0) {
        char *p = notice_buf;
        while (p && *p && row <= inner_rows) {
            char *nl = strchr(p, '\n');
            if (nl) *nl = '\0';
            mvwprintw(win, row++, 4, "%s", p);
            if (!nl) break;
            p = nl + 1;
        }
    } else {
        mvwprintw(win, row++, 4, "(none)");
    }

    if (row <= inner_rows) {
        mvwprintw(win, row++, 2, "");
    }

    /* Private messages section */
    if (row <= inner_rows) {
        mvwprintw(win, row++, 2, "Messages:");
        char msg_buf[1024];
        msg_buf[0] = '\0';
        int msg_limit = inner_rows - row;
        if (msg_limit < 1) msg_limit = 1;
        int msg_len = message_recent_to_buf(user->name, msg_limit, msg_buf, sizeof(msg_buf));
        if (msg_len > 0) {
            char *m = msg_buf;
            while (m && *m && row <= inner_rows) {
                char *nl = strchr(m, '\n');
                if (nl) *nl = '\0';
                mvwprintw(win, row++, 4, "%s", m);
                if (!nl) break;
                m = nl + 1;
            }
        } else {
            mvwprintw(win, row++, 4, "(no messages yet)");
        }
    }

    wrefresh(win);
}

static void draw_dashboard(User *user, const char *status) {
    erase();
    mvprintw(1, (COLS - 30) / 2, "Class Royale - Student Dashboard");
    mvprintw(3, 2, "Name: %s | Deposit: %d Cr | Cash: %d Cr | Rating: %c", user->name, user->bank.balance, user->bank.cash, user->bank.rating);
    mvprintw(3, 2, "Name: %s | Deposit: %d Cr | Cash: %d Cr | Rating: %c", user->name, user->bank.balance, user->bank.cash, user->bank.rating);
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
    mvwprintw(account_win, 1, 2, "Deposit: %d Cr", user->bank.balance);
    mvwprintw(account_win, 2, 2, "Cash: %d Cr", user->bank.cash);
    mvwprintw(account_win, 3, 2, "Loan: %d Cr", user->bank.loan);
    mvwprintw(account_win, 4, 2, "Estimated Tax: %d Cr", econ_tax(&user->bank));
    mvwprintw(account_win, 5, 2, "Recent Transactions");
    mvwprintw(account_win, 1, 2, "Deposit: %d Cr", user->bank.balance);
    mvwprintw(account_win, 2, 2, "Cash: %d Cr", user->bank.cash);
    mvwprintw(account_win, 3, 2, "Loan: %d Cr", user->bank.loan);
    mvwprintw(account_win, 4, 2, "Estimated Tax: %d Cr", econ_tax(&user->bank));
    mvwprintw(account_win, 5, 2, "Recent Transactions");
    char txbuf[2048];
    int got = account_recent_tx(user->name, 6, txbuf, sizeof(txbuf));
    if (got > 0) {
        int row = 6;
        int row = 6;
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

    WINDOW *news_win = tui_common_create_box(box_height, col_width, 7 + box_height, col_width + 4, "Notices [n]");
    render_news(news_win, user);
    tui_common_destroy_box(news_win);

    tui_common_draw_help("m:Missions s:Shop a:Account d:QOTD n:Messages q:Logout");
    tui_ncurses_draw_status(status);
    refresh();
}

static void handle_mission_board(User *user) {
    if (!user) {
        return;
    }

    /* ensure the latest missions from data/missions.csv are loaded,
       and refresh the user's mission list before showing the board */
    mission_refresh_catalog();
    mission_load_user(user->name, user);

    /* assign any newly opened missions (avoid duplicates via user_has_mission) */
    Mission open[8];
    int open_count = 0;
    if (mission_list_open(open, &open_count)) {
        for (int i = 0; i < open_count && i < 4; ++i) {
            if (!user_has_mission(user, open[i].id)) {
                admin_assign_mission(user->name, &open[i]);
            }
        }
    }


    /* ensure the latest missions from data/missions.csv are loaded,
       and refresh the user's mission list before showing the board */
    mission_refresh_catalog();
    mission_load_user(user->name, user);

    /* assign any newly opened missions (avoid duplicates via user_has_mission) */
    Mission open[8];
    int open_count = 0;
    if (mission_list_open(open, &open_count)) {
        for (int i = 0; i < open_count && i < 4; ++i) {
            if (!user_has_mission(user, open[i].id)) {
                admin_assign_mission(user->name, &open[i]);
            }
        }
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
            int mid = user->missions[highlight].id;
            if (mission_complete(user->name, mid)) {
                tui_ncurses_toast("Mission complete! Reward granted", 900);
                user_update_balance(user->name, user->bank.balance);

                /* reload user's missions so completed flag and counts update immediately */
                mission_load_user(user->name, user);

                /* adjust available/highlight after reload */
                available = user->mission_count;
                if (available == 0) {
                    highlight = 0;
                } else if (highlight >= available) {
                    highlight = available - 1;
                }

                /* reload user's missions so completed flag and counts update immediately */
                mission_load_user(user->name, user);

                /* adjust available/highlight after reload */
                available = user->mission_count;
                if (available == 0) {
                    highlight = 0;
                } else if (highlight >= available) {
                    highlight = available - 1;
                }
            } else {
                /* Diagnose likely reason and show clearer message */
                int found = 0;
                int already = 0;
                for (int ii = 0; ii < user->mission_count; ++ii) {
                    if (user->missions[ii].id == mid) {
                        found = 1;
                        if (user->missions[ii].completed) already = 1;
                        break;
                    }
                }
                if (!found) {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "Failed: mission #%d not assigned", mid);
                    tui_ncurses_toast(msg, 1200);
                } else if (already) {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "Already completed: mission #%d", mid);
                    tui_ncurses_toast(msg, 1200);
                } else {
                    tui_ncurses_toast("Failed to complete mission (internal)", 1200);
                }
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
                user_update_balance(user->name, user->bank.balance);
                /* refresh the shop list to reflect updated stock */
                if (shop_list(shops, &count) && count > 0) {
                    shop = &shops[0];
                    if (highlight >= shop->item_count) {
                        highlight = shop->item_count > 0 ? shop->item_count - 1 : 0;
                    }
                }
            } else {
                tui_ncurses_toast("Purchase failed - check balance/stock", 800);
            }
        } else if (ch == 's' || ch == 'S') {
            if (shop_sell(user->name, &shop->items[highlight], 1)) {
                tui_ncurses_toast("Sale complete", 800);
                /* refresh the shop list after sale as well */
                if (shop_list(shops, &count) && count > 0) {
                    shop = &shops[0];
                    if (highlight >= shop->item_count) {
                        highlight = shop->item_count > 0 ? shop->item_count - 1 : 0;
                    }
                }
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
                                        "Account Management (d deposit-from-cash / b borrow / r repay / w withdraw / q close)");
                                        "Account Management (d deposit-from-cash / b borrow / r repay / w withdraw / q close)");
    int running = 1;
    while (running) {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 0, 2, " Account Management ");
        mvwprintw(win, 1, 2, "Deposit: %d Cr", user->bank.balance);
        mvwprintw(win, 2, 2, "Cash: %d Cr", user->bank.cash);
        mvwprintw(win, 3, 2, "Loan: %d Cr", user->bank.loan);
        mvwprintw(win, 4, 2, "Rating: %c", user->bank.rating);
        mvwprintw(win, 5, 2, "Estimated Tax: %d Cr", econ_tax(&user->bank));
        mvwprintw(win, 7, 2, "Commands: d)deposit-from-cash  b)borrow  r)repay  w)withdraw-to-cash  q)close");
        mvwprintw(win, 2, 2, "Cash: %d Cr", user->bank.cash);
        mvwprintw(win, 3, 2, "Loan: %d Cr", user->bank.loan);
        mvwprintw(win, 4, 2, "Rating: %c", user->bank.rating);
        mvwprintw(win, 5, 2, "Estimated Tax: %d Cr", econ_tax(&user->bank));
        mvwprintw(win, 7, 2, "Commands: d)deposit-from-cash  b)borrow  r)repay  w)withdraw-to-cash  q)close");
        wrefresh(win);
        int ch = wgetch(win);
        if (ch == 'd' || ch == 'b' || ch == 'r' || ch == 'w') {
        if (ch == 'd' || ch == 'b' || ch == 'r' || ch == 'w') {
            char label[32];
                if (ch == 'd') {
                snprintf(label, sizeof(label), "Amount to deposit from cash");
                snprintf(label, sizeof(label), "Amount to deposit from cash");
            } else if (ch == 'b') {
                snprintf(label, sizeof(label), "Loan amount (take loan)");
            } else if (ch == 'r') {
                snprintf(label, sizeof(label), "Amount to repay loan");
            } else if (ch == 'w') {
                snprintf(label, sizeof(label), "Amount to withdraw to cash");
                snprintf(label, sizeof(label), "Loan amount (take loan)");
            } else if (ch == 'r') {
                snprintf(label, sizeof(label), "Amount to repay loan");
            } else if (ch == 'w') {
                snprintf(label, sizeof(label), "Amount to withdraw to cash");
            } else {
                snprintf(label, sizeof(label), "Amount");
                snprintf(label, sizeof(label), "Amount");
            }
            int amount = 0;
            if (tui_ncurses_prompt_number(win, label, &amount) && amount > 0) {
                int ok = 0;
                if (ch == 'd') {
                    /* move from cash -> deposit */
                    ok = account_deposit_from_cash(user, amount, "DEPOSIT_FROM_CASH");
                    /* move from cash -> deposit */
                    ok = account_deposit_from_cash(user, amount, "DEPOSIT_FROM_CASH");
                } else if (ch == 'b') {
                    /* take loan: loan += amount, cash += amount */
                    ok = account_take_loan(user, amount, "LOAN_TAKEN");
                } else if (ch == 'r') {
                    /* repay loan: loan -= amount, cash -= amount */
                    ok = account_repay_loan(user, amount, "LOAN_REPAY");
                } else if (ch == 'w') {
                    /* withdraw from deposit to cash */
                    ok = account_withdraw_to_cash(user, amount, "WITHDRAW_TO_CASH");
                    /* take loan: loan += amount, cash += amount */
                    ok = account_take_loan(user, amount, "LOAN_TAKEN");
                } else if (ch == 'r') {
                    /* repay loan: loan -= amount, cash -= amount */
                    ok = account_repay_loan(user, amount, "LOAN_REPAY");
                } else if (ch == 'w') {
                    /* withdraw from deposit to cash */
                    ok = account_withdraw_to_cash(user, amount, "WITHDRAW_TO_CASH");
                }
                if (ok) user_update_balance(user->name, user->bank.balance);
                tui_ncurses_toast(ok ? "Processed" : "Transaction failed", 800);
            }
        } else if (ch == 'q' || ch == 27) {
            running = 0;
        }
    }
    tui_common_destroy_box(win);
}

static void trim_whitespace(char *text) {
    if (!text) return;
    /* trim leading */
    char *start = text;
    while (*start == ' ' || *start == '\t') start++;
    if (start != text) memmove(text, start, strlen(start) + 1);
    /* trim trailing */
    size_t len = strlen(text);
    while (len > 0 && (text[len - 1] == ' ' || text[len - 1] == '\t')) {
        text[len - 1] = '\0';
        len--;
    }
}

static void handle_message_center(User *user) {
    if (!user) return;
    int height = LINES - 4;
    int width = COLS - 6;
    WINDOW *win = tui_common_create_box(height, width, (LINES - height) / 2, (COLS - width) / 2,
                                        "Notices/Messages (c compose / v view / a all / q close)");
    keypad(win, TRUE);
    int running = 1;
    char current_peer[50];
    current_peer[0] = '\0';
    while (running) {
        werase(win);
        box(win, 0, 0);
        int maxy = getmaxy(win);
        int content_limit = maxy - 3; /* keep last two rows for commands */
        int row = 1;
        mvwprintw(win, row++, 2, "Inbox for %s", user->name);

        char partners[32][50];
        int partner_count = message_list_partners(user->name, partners, 32);
        if (partner_count > 0) {
            mvwprintw(win, row++, 2, "Conversations:");
            for (int i = 0; i < partner_count && row <= content_limit; ++i) {
                int is_active = current_peer[0] && strncmp(current_peer, partners[i], sizeof(partners[i])) == 0;
                mvwprintw(win, row++, 4, "%c %s", is_active ? '*' : '-', partners[i]);
            }
        } else {
            mvwprintw(win, row++, 2, "No private conversations yet.");
        }
        if (row <= content_limit) {
            row++;
        }

        char feed[4096];
        feed[0] = '\0';
        int available_lines = content_limit - row;
        if (available_lines < 3) available_lines = 3;
        if (current_peer[0]) {
            mvwprintw(win, row++, 2, "Conversation with %s", current_peer);
            message_thread_to_buf(user->name, current_peer, available_lines, feed, sizeof(feed));
        } else {
            mvwprintw(win, row++, 2, "Recent messages");
            message_recent_to_buf(user->name, available_lines, feed, sizeof(feed));
        }

        char *p = feed;
        while (p && *p && row <= content_limit) {
            char *nl = strchr(p, '\n');
            if (nl) *nl = '\0';
            mvwprintw(win, row++, 2, "%s", p);
            if (!nl) break;
            p = nl + 1;
        }
        if (feed[0] == '\0' && row <= content_limit) {
            mvwprintw(win, row++, 2, "No messages to display.");
        }

        if (row <= content_limit) {
            mvwprintw(win, row++, 2, "Latest notices:");
            char notice_buf[1024];
            notice_buf[0] = '\0';
            int notice_limit = content_limit - row;
            if (notice_limit < 1) notice_limit = 1;
            notify_recent_to_buf(user->name, notice_limit, notice_buf, sizeof(notice_buf));
            char *nb = notice_buf;
            while (nb && *nb && row <= content_limit) {
                char *nl = strchr(nb, '\n');
                if (nl) *nl = '\0';
                mvwprintw(win, row++, 4, "%s", nb);
                if (!nl) break;
                nb = nl + 1;
            }
            if (notice_buf[0] == '\0' && row <= content_limit) {
                mvwprintw(win, row++, 4, "(none)");
            }
        }

        mvwprintw(win, maxy - 3, 2, current_peer[0] ? "Viewing conversation - press 'a' to show all" : "Viewing all messages");
        mvwprintw(win, maxy - 2, 2, "Commands: c)compose  v)view user  a)show all  q)close");
        wrefresh(win);

        int ch = wgetch(win);
        if (ch == 'q' || ch == 'Q' || ch == 27) {
            running = 0;
        } else if (ch == 'a' || ch == 'A') {
            current_peer[0] = '\0';
        } else if (ch == 'v' || ch == 'V') {
            char peer[50];
            memset(peer, 0, sizeof(peer));
            if (tui_ncurses_prompt_line(win, 2, 2, "View conversation with", peer, sizeof(peer), 0) && peer[0]) {
                trim_whitespace(peer);
                if (peer[0]) {
                    snprintf(current_peer, sizeof(current_peer), "%s", peer);
                }
            }
        } else if (ch == 'c' || ch == 'C') {
            char target[50];
            char message[200];
            memset(target, 0, sizeof(target));
            memset(message, 0, sizeof(message));
            if (!tui_ncurses_prompt_line(win, 2, 2, "Send to (username)", target, sizeof(target), 0) || target[0] == '\0') {
                continue;
            }
            trim_whitespace(target);
            if (strcmp(target, user->name) == 0) {
                tui_ncurses_toast("Cannot message yourself", 900);
                continue;
            }
            if (!user_lookup(target)) {
                tui_ncurses_toast("User not found", 900);
                continue;
            }
            if (!tui_ncurses_prompt_line(win, 3, 2, "Message", message, sizeof(message), 0) || message[0] == '\0') {
                tui_ncurses_toast("Message canceled", 700);
                continue;
            }
            trim_whitespace(message);
            if (message_send(user->name, target, message)) {
                tui_ncurses_toast("Message sent", 800);
                snprintf(current_peer, sizeof(current_peer), "%s", target);
            } else {
                tui_ncurses_toast("Failed to send message", 900);
            }
        }
    }
    tui_common_destroy_box(win);
}

void tui_student_loop(User *user) {
    if (!user) {
        return;
    }
    /* ensure latest global mission catalog is loaded at login so dashboard and mission board show new missions */
    mission_refresh_catalog();
     ensure_student_seed(user);
     const char *status = "Shortcut Keys";
     int running = 1;
     while (running) {
         draw_dashboard(user, status);
         int ch = getch();
         switch (ch) {
    /* ensure latest global mission catalog is loaded at login so dashboard and mission board show new missions */
    mission_refresh_catalog();
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
                handle_message_center(user);
                status = "Checked messages";
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
