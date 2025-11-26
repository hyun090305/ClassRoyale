#include "../../include/ui/tui_student.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#ifdef _WIN32
#include <process.h> /* _getpid */
#define GETPID() _getpid()
#else
#include <unistd.h>  /* getpid */
#define GETPID() getpid()
#endif

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
#include "../../include/domain/qotd.h"


#define ACCOUNT_STATS_MAX_TX 256

typedef struct {
    long timestamp;
    long total_asset;
} AssetPoint;

#include <stdbool.h>   // bool 쓸 거면 필요

// 이 파일 안에서만 쓰고 싶다면 둘 다 static
static void handle_class_seats_view(User *user);
static void handle_tutorial_view(User *user);

/* forward declarations for mission play screens - MUST be before handle_mission_board */
static void handle_mission_play_typing(User *user, Mission *m);
static void handle_mission_play_math(User *user, Mission *m);
static void handle_stocks_view(User *user);
static void handle_account_statistics(User *user);
static void handle_transactions_view(User *user);
static void handle_stock_graph_view(const Stock *stock);

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


static void qotd_runtime_clear(void) {
    for (int i = 0; i < qotd_solved_count; ++i) {
        if (qotd_solved_users[i]) free(qotd_solved_users[i]);
        qotd_solved_users[i] = NULL;
    }
    qotd_solved_count = 0;
}

static void qotd_runtime_mark_solved(const char *name) {
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
        /* load today's solved users into runtime cache */
    char today[32];
    time_t tnow = time(NULL);
    struct tm *tmnow = localtime(&tnow);
    if (tmnow) {
        strftime(today, sizeof(today), "%Y-%m-%d", tmnow);
        qotd_runtime_clear();
        char **users = NULL;
        int ucount = 0;
        if (qotd_get_solved_users_for_date(today, &users, &ucount)) {
            for (int i = 0; i < ucount; ++i) {
                if (users[i]) qotd_runtime_mark_solved(users[i]);
                free(users[i]);
            }
            free(users);
        }
    }
    if (qotd_is_solved(user->name)) {
        tui_ncurses_toast("QOTD already solved", 900);
        return;
    }

    QOTD q = {0};
    int has_q = qotd_get_today(&q);
    const char *question = has_q ? q.question : "(no QOTD today)";
    const char *opts[3];
    if (has_q) {
        opts[0] = q.opt1[0] ? q.opt1 : "(no option1)";
        opts[1] = q.opt2[0] ? q.opt2 : "(no option2)";
        opts[2] = q.opt3[0] ? q.opt3 : "(no option3)";
    } else {
        opts[0] = "-";
        opts[1] = "-";
        opts[2] = "-";
    }
    const int correct_choice = has_q ? q.right_index : 1; /* 1-based index of correct option */
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
        for (int i = 0; i < 3; ++i) {
            mvwprintw(win, 3 + i, 4, "%d) %s", i + 1, opts[i]);
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
                /* grant cash directly and persist tx (award to on-hand cash) */
                int ok = account_grant_cash(user, reward, "QOTD_REWARD");
                if (ok) {
                    /* persist solved entry for today */
                    if (tmnow) {
                        /* persist solved entry via domain API */
                        qotd_mark_solved(user->name);
                    }
                    /* mark in-memory runtime cache so UI hides QOTD */
                    qotd_runtime_mark_solved(user->name);
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
        QOTD tq = {0};
        if (qotd_get_today(&tq)) {
            mvwprintw(win, getmaxy(win) - 4, 2, "QOTD: %s", tq.name);
            mvwprintw(win, getmaxy(win) - 3, 4, "1) %s  2) %s  3) %s", tq.opt1[0] ? tq.opt1 : "-", tq.opt2[0] ? tq.opt2 : "-", tq.opt3[0] ? tq.opt3 : "-");
            mvwprintw(win, getmaxy(win) - 2, 4, "[d] Respond on submission screen");
        } else {
            mvwprintw(win, getmaxy(win) - 4, 2, "QOTD: (none today)");
            mvwprintw(win, getmaxy(win) - 3, 4, "");
            mvwprintw(win, getmaxy(win) - 2, 4, "");
        }
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
    /* refresh today's QOTD runtime cache so hints reflect persisted state */
    if (user) {
        char today[32];
        time_t tnow = time(NULL);
        struct tm *tmnow = localtime(&tnow);
        if (tmnow) {
            strftime(today, sizeof(today), "%Y-%m-%d", tmnow);
            qotd_runtime_clear();
            char **users = NULL;
            int ucount = 0;
            if (qotd_get_solved_users_for_date(today, &users, &ucount)) {
                for (int i = 0; i < ucount; ++i) {
                    if (users[i]) qotd_runtime_mark_solved(users[i]);
                    free(users[i]);
                }
                free(users);
            }
        }
    }
    mvprintw(1, (COLS - 30) / 2, "Class Royale - Student Dashboard");
    mvprintw(3, 2, "Name: %s | Deposit: %d Cr | Cash: %d Cr", user->name, user->bank.balance, user->bank.cash);
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

    WINDOW *account_win = tui_common_create_box(box_height, col_width, 7 + box_height, 2, "Account Status [a]");
    mvwprintw(account_win, 1, 2, "Deposit: %d Cr", user->bank.balance);
    mvwprintw(account_win, 2, 2, "Cash: %d Cr", user->bank.cash);
    mvwprintw(account_win, 3, 2, "Loan: %d Cr", user->bank.loan);
    mvwprintw(account_win, 5, 2, "Recent Transactions [t]");
    mvwprintw(account_win, 1, 2, "Deposit: %d Cr", user->bank.balance);
    mvwprintw(account_win, 2, 2, "Cash: %d Cr", user->bank.cash);
    mvwprintw(account_win, 3, 2, "Loan: %d Cr", user->bank.loan);
    mvwprintw(account_win, 5, 2, "Recent Transactions");
    char txbuf[2048];
    int got = account_recent_tx(user->name, 6, txbuf, sizeof(txbuf));
    if (got > 0) {
        int row = 6;
        /* split buffer into lines, collect pointers then print newest-first */
        char *lines[256];
        int line_count = 0;
        char *p = txbuf;
        while (p && *p && line_count < (int)(sizeof(lines)/sizeof(lines[0]))) {
            char *nl = strchr(p, '\n');
            if (nl) *nl = '\0';
            lines[line_count++] = p;
            if (!nl) break;
            p = nl + 1;
        }
        /* print in reverse so the most recent transaction appears first
           and truncate each line to the account window width to avoid
           automatic wrapping. */
        int win_w = getmaxx(account_win);
        int max_print = win_w - 6; /* 4 col offset + small margin */
        if (max_print < 1) max_print = 1;
        for (int i = line_count - 1; i >= 0 && row < getmaxy(account_win) - 1; --i) {
            mvwprintw(account_win, row++, 4, "%.*s", max_print, lines[i]);
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

    tui_common_draw_help("m:Missions s:Shop a:Account t:Transactions d:QOTD n:Messages r:Tutorial q:Logout");
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
            Mission *sel = &user->missions[highlight];
            if (sel->completed) {
                tui_ncurses_toast("Mission already completed", 800);
            } else {
                /* launch appropriate play UI; UI will call mission_complete when finished */
                if (sel->type == 0) {
                    handle_mission_play_typing(user, sel);
                } else {
                    handle_mission_play_math(user, sel);
                }
                /* reload user's missions so completion / rewards show immediately */
                mission_load_user(user->name, user);
                available = user->mission_count;
                if (available == 0) {
                    highlight = 0;
                } else if (highlight >= available) {
                    highlight = available - 1;
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
    int width  = COLS - 6;

    WINDOW *win = tui_common_create_box(
        height,
        width,
        2,
        3,
        "Shop (Enter=Buy / s=Sell / Stocks / Class Seats / q=Close)"
    );

    keypad(win, TRUE);

    int highlight = 0;      // 선택 인덱스 (0 ~ item_count-1 = 아이템, item_count = Stocks, item_count+1 = Class Seats)
    int running   = 1;

    while (running) {
        werase(win);
        box(win, 0, 0);

        mvwprintw(win, 0, 2,
                  " %s Shop - Cash %dCr ",
                  shop->name,
                  user->bank.cash);

        /* ------------------------- 인덱스 설계 ------------------------- */
        int idx_stock_btn = shop->item_count;        // [ Stocks ]
        int idx_class_btn = shop->item_count + 1;    // [ Class Seats ]
        int total_choices = shop->item_count + 2;    // 아이템 + 2 버튼

        /* ------------------------- 아이템 리스트 ------------------------ */
        int list_start_row = 1;
        int stock_line     = height - 3;
        int class_line     = height - 2;

        if (stock_line <= list_start_row)
            stock_line = list_start_row + 1;
        if (class_line <= stock_line)
            class_line = stock_line + 1;

        int list_max_rows = stock_line - list_start_row;  // 버튼 위까지 사용

        int items_to_show = shop->item_count;
        if (items_to_show > list_max_rows) {
            items_to_show = list_max_rows;
        }

        for (int i = 0; i < items_to_show; ++i) {
            if (i == highlight) {
                wattron(win, A_REVERSE);
            }

            mvwprintw(
                win,
                list_start_row + i,
                2,
                "%-16s %4dCr  Stock:%2d",
                shop->items[i].name,
                shop->items[i].cost,
                shop->items[i].stock
            );

            if (i == highlight) {
                wattroff(win, A_REVERSE);
            }
        }

        if (shop->item_count == 0) {
            mvwprintw(win, list_start_row, 2, "No items registered");
        }

        /* ------------------------- 아래 버튼 2개 ------------------------ */

        // [ Stocks ] 버튼
        if (highlight == idx_stock_btn) {
            wattron(win, A_REVERSE);
        }
        mvwprintw(win, stock_line, 2, "[ Stocks ]  (Enter)");
        if (highlight == idx_stock_btn) {
            wattroff(win, A_REVERSE);
        }

        // [ Class Seats ] 버튼
        if (highlight == idx_class_btn) {
            wattron(win, A_REVERSE);
        }
        mvwprintw(win, class_line, 2, "[ Class Seats ]  (Enter)");
        if (highlight == idx_class_btn) {
            wattroff(win, A_REVERSE);
        }

        wrefresh(win);

        /* ------------------------- 입력 처리 ---------------------------- */
        int ch = wgetch(win);

        if (ch == KEY_UP) {
            highlight = (highlight - 1 + total_choices) % total_choices;
        } else if (ch == KEY_DOWN) {
            highlight = (highlight + 1) % total_choices;
        } else if (ch == '\n' || ch == '\r') {
            /* 버튼들 먼저 처리 */
            if (highlight == idx_stock_btn) {
                // 주식 화면
                handle_stocks_view(user);
                // 돌아오면 상점 화면 계속
                continue;
            }

            if (highlight == idx_class_btn) {
                // 클래스 좌석 화면
                handle_class_seats_view(user);
                continue;
            }

            /* 아이템 구매 */
            if (highlight < shop->item_count) {
                Item *it = &shop->items[highlight];

                if (shop_buy(user->name, it, 1)) {
                    tui_ncurses_toast("Purchase complete", 800);
                    user_update_balance(user->name, user->bank.balance);

                    // CSV 수량 1 감소
                    if (!shop_decrease_stock_csv(it->name)) {
                        tui_ncurses_toast("Warning: CSV update failed", 800);
                    }

                    // 메모리 상 재고 감소
                    if (it->stock > 0) {
                        it->stock--;
                    }

                    // 상점 데이터 다시 로드 (재고/정렬 반영)
                    if (shop_list(shops, &count) && count > 0) {
                        shop = &shops[0];

                        // 아이템 개수가 줄었을 수 있으니 highlight 조정
                        if (highlight >= shop->item_count) {
                            highlight = shop->item_count - 1;
                            if (highlight < 0) {
                                highlight = 0;
                            }
                        }
                    }
                } else {
                    tui_ncurses_toast("Purchase failed - check balance/stock", 800);
                }
            }
        } else if (ch == 's' || ch == 'S') {
            // 아이템 판매는 아이템 줄에서만 가능
            if (highlight < shop->item_count) {
                if (shop_sell(user->name, &shop->items[highlight], 1)) {
                    tui_ncurses_toast("Sale complete", 800);

                    if (shop_list(shops, &count) && count > 0) {
                        shop = &shops[0];
                        if (highlight >= shop->item_count) {
                            highlight = shop->item_count - 1;
                            if (highlight < 0) {
                                highlight = 0;
                            }
                        }
                    }
                } else {
                    tui_ncurses_toast("Sale failed", 800);
                }
            }
        } else if (ch == 'q' || ch == 27) {
            running = 0;
        }
    }

    tui_common_destroy_box(win);
}

static int account_reason_matches(const char *reason, const char *prefix) {
    if (!reason || !prefix) return 0;
    size_t need = strlen(prefix);
    if (need == 0) return 0;
    return strncmp(reason, prefix, need) == 0;
}

static int collect_asset_points(User *user, AssetPoint *points, int max_points) {
    if (!user || !points || max_points <= 0) return 0;

    char path[512];
    snprintf(path, sizeof(path), "data/txs/%s.csv", user->name);

    char *raw = NULL;
    size_t raw_len = 0;
    if (!csv_read_last_lines(path, ACCOUNT_STATS_MAX_TX, &raw, &raw_len) || !raw) {
        if (raw) free(raw);
        return 0;
    }

    typedef struct {
        long ts;
        int amount;
        int balance;
        char reason[64];
    } AccountTxRow;

    AccountTxRow rows[ACCOUNT_STATS_MAX_TX];
    int row_count = 0;

    char *cursor = raw;
    while (cursor && *cursor && row_count < ACCOUNT_STATS_MAX_TX) {
        char *line = cursor;
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        if (*line) {
            char *tokens[4] = {0};
            int tokc = 0;
            char *tok = strtok(line, ",");
            while (tok && tokc < 4) {
                tokens[tokc++] = tok;
                tok = strtok(NULL, ",");
            }
            if (tokc >= 3) {
                AccountTxRow row = {0};
                row.ts = tokens[0] ? atol(tokens[0]) : 0;
                if (tokc == 4) {
                    snprintf(row.reason, sizeof(row.reason), "%s", tokens[1] ? tokens[1] : "");
                    row.amount = tokens[2] ? atoi(tokens[2]) : 0;
                    row.balance = tokens[3] ? atoi(tokens[3]) : 0;
                } else {
                    row.reason[0] = '\0';
                    row.amount = tokens[1] ? atoi(tokens[1]) : 0;
                    row.balance = tokens[2] ? atoi(tokens[2]) : 0;
                }
                rows[row_count++] = row;
            }
        }

        if (!nl) break;
        cursor = nl + 1;
    }

    free(raw);

    if (row_count == 0) return 0;

    long cash = user->bank.cash;
    long loan = user->bank.loan;

    AssetPoint reversed[ACCOUNT_STATS_MAX_TX];
    int rev_count = 0;

    for (int idx = row_count - 1; idx >= 0; --idx) {
        AccountTxRow *row = &rows[idx];
        long deposit = row->balance;
        long total = deposit + cash - loan;
        if (rev_count < ACCOUNT_STATS_MAX_TX) {
            reversed[rev_count].timestamp = row->ts;
            reversed[rev_count].total_asset = total;
            rev_count++;
        }

        if (account_reason_matches(row->reason, "DEPOSIT_FROM_CASH")) {
            cash += row->amount;
        } else if (account_reason_matches(row->reason, "WITHDRAW_TO_CASH")) {
            cash += row->amount;
        } else if (account_reason_matches(row->reason, "LOAN_REPAY")) {
            long delta = -(long)row->amount;
            cash += delta;
            loan += delta;
        } else if (account_reason_matches(row->reason, "LOAN_TAKEN") ||
                   account_reason_matches(row->reason, "LOAN")) {
            cash -= row->amount;
            loan -= row->amount;
        }
    }

    if (rev_count == 0) return 0;

    int copy = rev_count < max_points ? rev_count : max_points;
    for (int i = 0; i < copy; ++i) {
        points[i] = reversed[copy - 1 - i];
    }
    return copy;
}

static void handle_account_statistics(User *user) {
    if (!user) return;

    int height = LINES - 6;
    if (height < 12) height = 12;
    if (height > LINES - 2) height = LINES - 2;
    int width = COLS - 8;
    if (width < 48) width = 48;
    if (width > COLS - 2) width = COLS - 2;

    int starty = (LINES - height) / 2;
    int startx = (COLS - width) / 2;

    WINDOW *win = tui_common_create_box(height, width, starty, startx,
                                        "Account Statistics (q to close)");
    if (!win) return;
    keypad(win, TRUE);

    AssetPoint points[ACCOUNT_STATS_MAX_TX];
    int point_count = collect_asset_points(user, points, ACCOUNT_STATS_MAX_TX);

    if (point_count == 0) {
        mvwprintw(win, 2, 2, "No transaction history to chart.");
        mvwprintw(win, 3, 2, "Complete missions or trade to build stats.");
    } else {
        int usable_rows = height - 5;
        if (usable_rows < 1) usable_rows = 1;
        int start_idx = point_count > usable_rows ? point_count - usable_rows : 0;

        long min_total = points[start_idx].total_asset;
        long max_total = points[start_idx].total_asset;
        for (int i = start_idx; i < point_count; ++i) {
            if (points[i].total_asset < min_total) min_total = points[i].total_asset;
            if (points[i].total_asset > max_total) max_total = points[i].total_asset;
        }
        long range = max_total - min_total;
        if (range <= 0) range = 1;

        int label_width = 14;
        int bar_col = 2 + label_width + 1;
        int bar_width = width - bar_col - 12;
        if (bar_width < 10) bar_width = 10;

        int row = 2;
        for (int i = start_idx; i < point_count && row < height - 3; ++i, ++row) {
            char timebuf[32];
            if (points[i].timestamp > 0) {
                time_t tt = (time_t)points[i].timestamp;
                struct tm *tm = localtime(&tt);
                if (tm) {
                    strftime(timebuf, sizeof(timebuf), "%m-%d %H:%M", tm);
                } else {
                    snprintf(timebuf, sizeof(timebuf), "%ld", points[i].timestamp);
                }
            } else {
                snprintf(timebuf, sizeof(timebuf), "entry %d", i + 1);
            }
            mvwprintw(win, row, 2, "%-*s", label_width, timebuf);

            mvwhline(win, row, bar_col, ' ', bar_width);
            long long scaled = (long long)(points[i].total_asset - min_total) * bar_width;
            int filled = (int)(scaled / range);
            if (filled < 0) filled = 0;
            if (filled > bar_width) filled = bar_width;
            if (filled > 0) {
                mvwhline(win, row, bar_col, '#', filled);
            }
            mvwprintw(win, row, bar_col + bar_width + 1, "%ld", points[i].total_asset);
        }

        mvwprintw(win, height - 4, 2, "Newest total: %ld Cr | Peak: %ld Cr | Lowest: %ld Cr",
                  points[point_count - 1].total_asset, max_total, min_total);
    }

    mvwprintw(win, height - 2, 2, "Total = deposit + cash - loan. Press q / ESC to close.");
    wrefresh(win);

    int ch;
    while ((ch = wgetch(win)) != 'q' && ch != 'Q' && ch != 27) {
        /* wait */
    }
    tui_common_destroy_box(win);
}


static void handle_account_view(User *user) {
    int height = LINES - 4;
    int width = COLS - 6;
    WINDOW *win = tui_common_create_box(height, width, (LINES - height) / 2, (COLS - width) / 2,
                                        "Account Management (d deposit / b borrow / r repay / w withdraw / q close)");
    int running = 1;
    while (running) {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 0, 2, " Account Management ");
        mvwprintw(win, 1, 2, "Deposit: %d Cr", user->bank.balance);
        mvwprintw(win, 2, 2, "Cash: %d Cr", user->bank.cash);
        mvwprintw(win, 3, 2, "Loan: %d Cr", user->bank.loan);
        /* rating removed */
        mvwprintw(win, 5, 2, "Commands: d)deposit  w)withdraw  b)borrow  r)repay  q)close");
        wrefresh(win);
        int ch = wgetch(win);
        if (ch == 'd' || ch == 'b' || ch == 'r' || ch == 'w') {
            char label[32];
                if (ch == 'd') {
                snprintf(label, sizeof(label), "Amount to deposit from cash");
            } else if (ch == 'b') {
                snprintf(label, sizeof(label), "Loan amount (take loan)");
            } else if (ch == 'r') {
                snprintf(label, sizeof(label), "Amount to repay loan");
            } else if (ch == 'w') {
                snprintf(label, sizeof(label), "Amount to withdraw to cash");
            } else {
                snprintf(label, sizeof(label), "Amount");
            }
            int amount = 0;
            if (tui_ncurses_prompt_number(win, label, &amount) && amount > 0) {
                int ok = 0;
                if (ch == 'd') {
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

static void handle_transactions_view(User *user) {
    if (!user) return;

    int height = LINES - 4;
    int width = COLS - 6;
    WINDOW *win = tui_common_create_box(height, width, (LINES - height) / 2, (COLS - width) / 2,
                                        "Transactions (t:stats / q:close)");
    keypad(win, TRUE);

    char txbuf[8192];
    txbuf[0] = '\0';
    int got = account_recent_tx(user->name, ACCOUNT_STATS_MAX_TX, txbuf, sizeof(txbuf));

    char *lines[2048];
    int line_count = 0;
    if (got > 0) {
        char *p = txbuf;
        while (p && *p && line_count < (int)(sizeof(lines)/sizeof(lines[0]))) {
            char *nl = strchr(p, '\n');
            if (nl) *nl = '\0';
            lines[line_count++] = p;
            if (!nl) break;
            p = nl + 1;
        }
        /* reverse order so newest transactions appear first */
        for (int i = 0; i < line_count / 2; ++i) {
            char *tmp = lines[i];
            lines[i] = lines[line_count - 1 - i];
            lines[line_count - 1 - i] = tmp;
        }
    }

    int start = 0;
    int inner_rows = height - 3;
    int win_w = getmaxx(win);
    int max_print = win_w - 4;
    if (max_print < 1) max_print = 1;

    int running = 1;
    while (running) {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 0, 2, " Transactions (t:stats / q:close) ");

        if (line_count == 0) {
            mvwprintw(win, 2, 2, "No transactions");
        } else {
            for (int i = 0; i < inner_rows && (start + i) < line_count; ++i) {
                mvwprintw(win, 1 + i, 2, "%.*s", max_print, lines[start + i]);
            }
            mvwprintw(win, height - 2, 2, "Up/Down/PageUp/PageDown to scroll. t:stats q:close");
        }

        wrefresh(win);

        int ch = wgetch(win);
        if (ch == KEY_UP) {
            if (start > 0) start--;
        } else if (ch == KEY_DOWN) {
            if (start + inner_rows < line_count) start++;
        } else if (ch == KEY_NPAGE) {
            start += inner_rows;
            if (start + inner_rows > line_count) start = line_count - inner_rows;
            if (start < 0) start = 0;
        } else if (ch == KEY_PPAGE) {
            start -= inner_rows;
            if (start < 0) start = 0;
        } else if (ch == 'q' || ch == 27) {
            running = 0;
        } else if (ch == 't' || ch == 'T') {
            handle_account_statistics(user);
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
        /* Show latest notices first (top) */
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

        /* gap between sections */
        if (row <= content_limit) row++;

        /* Conversations / partners */
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

        /* Message feed: either specific thread or recent messages */
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
            int prompt_y = maxy - 4;
            if (prompt_y < 1) prompt_y = 1;
            if (tui_ncurses_prompt_line(win, prompt_y, 2, "View conversation with", peer, sizeof(peer), 0) && peer[0]) {
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
            int prompt_y = maxy - 4;
            if (prompt_y < 1) prompt_y = 1;
            if (!tui_ncurses_prompt_line(win, prompt_y, 2, "Send to (username)", target, sizeof(target), 0) || target[0] == '\0') {
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
            if (!tui_ncurses_prompt_line(win, prompt_y-1, 2, "Message", message, sizeof(message), 0) || message[0] == '\0') {
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
            case 't':
            case 'T':
                handle_transactions_view(user);
                status = "Viewed transactions";
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
            case 'r':
            case 'R':
                handle_tutorial_view(user);
                status = "Viewed tutorial";
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

/* forward declarations for mission play screens */
static void handle_mission_play_typing(User *user, Mission *m);
static void handle_mission_play_math(User *user, Mission *m);

typedef struct {
    char name[64];
} Seat;

static Seat g_seats[31]; // 1~30까지 사용

static void load_seats_csv(void) {
    FILE *fp = fopen("data/seats.csv", "r");
    if (!fp) return;

    int num;
    char name[64];

    while (fscanf(fp, "%d,%63[^\n]\n", &num, name) != EOF) {
        strcpy(g_seats[num].name, name);
    }
    fclose(fp);
}
void save_seats_csv(void) {
    FILE *fp = fopen("data/seats.csv", "w");
    if (!fp) return;

    for (int i = 1; i <= 30; i++) {
        fprintf(fp, "%d,%s\n", i, g_seats[i].name);
    }

    fclose(fp);
}

/* --- Class seats view (stub) --- */
static void handle_class_seats_view(User *user) {
    int height = LINES - 6;
    int width  = COLS - 10;

    WINDOW *win = tui_common_create_box(
        height,
        width,
        3,
        5,
        "Class Seats (q to close)"
    );

    keypad(win, TRUE);

    int cursor = 1;  // 선택된 좌석 번호
    int running = 1;

    while (running) {
        werase(win);
        box(win, 0, 0);

        mvwprintw(win, 1, 2,
            "Manage class seats for %s  (Arrow keys, Enter=reserve/cancel)",
            user->name);

        int start_y = 3;
        int start_x = 2;

        // ===== 좌석 출력 =====
        for (int row = 0; row < 6; row++) {
            for (int col = 0; col < 5; col++) {
                int seat_no = row * 5 + col + 1;

                if (seat_no == cursor)
                    wattron(win, A_REVERSE);

                char buf[128];
                if (strlen(g_seats[seat_no].name) == 0)
                    snprintf(buf, sizeof(buf), "[%2d: empty]", seat_no);
                else
                    snprintf(buf, sizeof(buf), "[%2d: %s]", seat_no, g_seats[seat_no].name);

                mvwprintw(win, start_y + row, start_x + col * 18, "%s", buf);

                if (seat_no == cursor)
                    wattroff(win, A_REVERSE);
            }
        }

        wrefresh(win);
        int ch = wgetch(win);

        // ===== 입력 처리 =====
        switch (ch) {
        case KEY_UP:
            if (cursor > 5) cursor -= 5;
            break;

        case KEY_DOWN:
            if (cursor <= 25) cursor += 5;
            break;

        case KEY_LEFT:
            if (((cursor - 1) % 5) != 0) cursor--;
            break;

        case KEY_RIGHT:
            if ((cursor % 5) != 0) cursor++;
            break;

        case '\n':
        case KEY_ENTER: {

            // ===== 먼저 본인이 이미 다른 좌석을 예약했는지 검사 =====
            int mySeat = -1;
            for (int i = 1; i <= 30; i++) {
                if (strcmp(g_seats[i].name, user->name) == 0) {
                    mySeat = i;
                    break;
                }
            }

            // == 1) 본인이 이미 좌석을 가지고 있다면 ==
            if (mySeat != -1 && mySeat != cursor) {
                mvwprintw(win, height - 3, 2,
                    "You already reserved seat %d. Cancel it first.", mySeat);
                wrefresh(win);
                napms(500);
                break;
            }

            // == 2) 본인이 자기 좌석을 선택한 경우 → 해제 ==
            if (mySeat == cursor) {
                g_seats[cursor].name[0] = '\0';    
                save_seats_csv();
                mvwprintw(win, height - 3, 2,
                    "Seat %d cancelled.", cursor);
                user->bank.cash += 1000;
                wrefresh(win);
                napms(500);
                break;
            }

            // == 3) 빈 좌석이면 예약 ==
            if (strlen(g_seats[cursor].name) == 0) {
                strcpy(g_seats[cursor].name, user->name);
                save_seats_csv();

                mvwprintw(win, height - 3, 2,
                    "Seat %d reserved for %s   ", cursor, user->name);
                user->bank.cash -= 1000;  
                wrefresh(win);
                napms(500);
            } else {
                mvwprintw(win, height - 3, 2,
                    "Seat %d is already reserved!", cursor);
                wrefresh(win);
                napms(500);
            }

            break;
        }

        case 'q':
        case 27:
            running = 0;
            break;
        }
    }

    tui_common_destroy_box(win);
}

/* Tutorial viewer: draws into provided WINDOW. This function lists three
 * choices (Missions & QOTD, Shop, Cash & Deposit). Use arrow keys and
 * Enter to open a topic, q to close the tutorial. Important keywords
 * are highlighted in yellow for easy reading by young students. */
static void handle_tutorial_view(User *user) {
    int height = LINES - 6;
    int width  = COLS - 10;
    WINDOW *win = tui_common_create_box(
        height,
        width,
        3,
        5,
        "Class Seats (q to close)"
    );
    if (!win) return;
    int maxy = getmaxy(win);
    int maxx = getmaxx(win);

    /* prepare a yellow highlight color pair if colors are available.
     * Use yellow background with black text for better visibility in some terminals. */
    if (has_colors()) {
        /* pair 2 reserved for tutorial highlight: yellow text on black background */
        init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    }

    const char *options[] = {"Missions & QOTD", "Shop", "Cash & Deposit"};
    int opt_count = 3;
    int highlight = 0;
    int running = 1;

    while (running) {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 0, 2, " Class Royale Tutorial ");
        mvwprintw(win, 1, 2, "Use Up/Down to choose, Enter to read, q to close");

        for (int i = 0; i < opt_count; ++i) {
            if (i == highlight) wattron(win, A_REVERSE);
            mvwprintw(win, 3 + i, 4, "%s", options[i]);
            if (i == highlight) wattroff(win, A_REVERSE);
        }

        wrefresh(win);
        int ch = wgetch(win);
        if (ch == KEY_UP) {
            highlight = (highlight - 1 + opt_count) % opt_count;
        } else if (ch == KEY_DOWN) {
            highlight = (highlight + 1) % opt_count;
        } else if (ch == '\n' || ch == '\r') {
            /* show selected topic */
            werase(win);
            box(win, 0, 0);
            if (highlight == 0) {
                mvwprintw(win, 0, 2, " Missions & QOTD ");
                mvwprintw(win, 2, 2, "Missions are small activities you can do to earn Cr.");
                /* print line with colored keyword */
                mvwprintw(win, 4, 2, "- ");
                /* print highlighted keyword then rest of line */
                if (has_colors()) {
                    wattron(win, COLOR_PAIR(2));
                    wattron(win, A_BOLD);
                } else {
                    wattron(win, A_UNDERLINE);
                }
                wprintw(win, "Missions");
                if (has_colors()) {
                    wattroff(win, A_BOLD);
                    wattroff(win, COLOR_PAIR(2));
                } else {
                    wattroff(win, A_UNDERLINE);
                }
                wprintw(win, ": complete fun tasks (typing, math) to get rewards.");

                mvwprintw(win, 5, 2, "- ");
                if (has_colors()) {
                    wattron(win, COLOR_PAIR(2));
                    wattron(win, A_BOLD);
                } else {
                    wattron(win, A_UNDERLINE);
                }
                wprintw(win, "QOTD");
                if (has_colors()) {
                    wattroff(win, A_BOLD);
                    wattroff(win, COLOR_PAIR(2));
                } else {
                    wattroff(win, A_UNDERLINE);
                }
                wprintw(win, ": a short quiz you can answer once each day.");

                mvwprintw(win, 7, 2, "When you finish a mission or QOTD you earn Cr (cash).");
                mvwprintw(win, 9, 2, "Try to do them every day to practice and collect rewards!");
            } else if (highlight == 1) {
                mvwprintw(win, 0, 2, " Shop ");
                mvwprintw(win, 2, 2, "The Shop is where you can spend Cr to buy items.");
                mvwprintw(win, 4, 2, "- Items: things you can use or show off (cost is shown).");

                mvwprintw(win, 6, 2, "- ");
                if (has_colors()) {
                    wattron(win, COLOR_PAIR(2));
                    wattron(win, A_BOLD);
                } else {
                    wattron(win, A_UNDERLINE);
                }
                wprintw(win, "Stocks");
                if (has_colors()) {
                    wattroff(win, A_BOLD);
                    wattroff(win, COLOR_PAIR(2));
                } else {
                    wattroff(win, A_UNDERLINE);
                }
                wprintw(win, ": tiny pieces of a company. Prices can go up or down.");

                mvwprintw(win, 8, 2, "- ");
                if (has_colors()) {
                    wattron(win, COLOR_PAIR(2));
                    wattron(win, A_BOLD);
                } else {
                    wattron(win, A_UNDERLINE);
                }
                wprintw(win, "Seat Coupons");
                if (has_colors()) {
                    wattroff(win, A_BOLD);
                    wattroff(win, COLOR_PAIR(2));
                } else {
                    wattroff(win, A_UNDERLINE);
                }
                wprintw(win, " : lets you reserve a special seat in class.");

                mvwprintw(win, 10, 2, "You can try to buy stocks to grow your Cr, but prices may fall too.");
            } else if (highlight == 2) {
                mvwprintw(win, 0, 2, " Cash & Deposit ");
                mvwprintw(win, 2, 2, "You get Cash when you finish missions or QOTD.");
                mvwprintw(win, 4, 2, "Deposit is money you put in the bank to keep it safe.");

                mvwprintw(win, 6, 2, "- ");
                if (has_colors()) {
                    wattron(win, COLOR_PAIR(2));
                    wattron(win, A_BOLD);
                } else {
                    wattron(win, A_UNDERLINE);
                }
                wprintw(win, "Cash");
                if (has_colors()) {
                    wattroff(win, A_BOLD);
                    wattroff(win, COLOR_PAIR(2));
                } else {
                    wattroff(win, A_UNDERLINE);
                }
                wprintw(win, " : money you carry and spend right away.");

                mvwprintw(win, 8, 2, "- ");
                if (has_colors()) {
                    wattron(win, COLOR_PAIR(2));
                    wattron(win, A_BOLD);
                } else {
                    wattron(win, A_UNDERLINE);
                }
                wprintw(win, "Deposit");
                if (has_colors()) {
                    wattroff(win, A_BOLD);
                    wattroff(win, COLOR_PAIR(2));
                } else {
                    wattroff(win, A_UNDERLINE);
                }
                wprintw(win, " : money you keep in the bank");

                mvwprintw(win, 10, 2, "The bank can give a small extra called interest over time.");
                mvwprintw(win, 12, 2, "That means your deposit can slowly grow, like a tiny present!");
            }
            mvwprintw(win, maxy - 2, 2, "Press any key to go back to the tutorial menu");
            wrefresh(win);
            wgetch(win);
        } else if (ch == 'q' || ch == 'Q' || ch == 27) {
            running = 0;
        }
    }
}
/* --- Mission play screens (typing practice / math quiz) --- */

static void append_typing_leaderboard(const char *username, int mission_id, double wpm, double accuracy) {
    csv_ensure_dir("data");
    FILE *fp = fopen("data/typing_leaderboard.csv", "a");
    if (!fp) return;
    /* format: username,mission_id,wpm,accuracy_percent */
    fprintf(fp, "%s,%d,%.2f,%.2f\n", username ? username : "unknown", mission_id, wpm, accuracy);
    fclose(fp);
}

static void append_math_leaderboard(const char *username, int mission_id, double total_seconds) {
    csv_ensure_dir("data");
    FILE *fp = fopen("data/math_leaderboard.csv", "a");
    if (!fp) return;
    fprintf(fp, "%s,%d,%.3f\n", username ? username : "unknown", mission_id, total_seconds);
    fclose(fp);
}

/* comparator for math leaderboard sort (ascending time) */
static int cmp_math_time(const void *a, const void *b) {
    const double ta = ((const struct { char name[128]; double time; } *)a)->time;
    const double tb = ((const struct { char name[128]; double time; } *)b)->time;
    if (ta < tb) return -1;
    if (ta > tb) return 1;
    return 0;
}

/* typing leaderboard entry type and comparator (WPM desc) */
typedef struct { char name[128]; double wpm; double acc; } typing_lbent;
static int cmp_typing_wpm_desc(const void *a, const void *b) {
    const typing_lbent *A = (const typing_lbent *)a;
    const typing_lbent *B = (const typing_lbent *)b;
    if (A->wpm < B->wpm) return 1;
    if (A->wpm > B->wpm) return -1;
    return 0;
}

/* Typing practice:
   - Two text lines shown (static per spec).
   - User types on the line below each text.
   - Incorrect characters are drawn with standout / red if available.
   - Backspace supported, Enter moves to next input line.
   - After finishing, compute elapsed, accuracy, WPM and write leaderboard.
   - Press Enter on summary to mark mission complete and exit.
*/
static void handle_mission_play_typing(User *user, Mission *m) {
    if (!user || !m) return;

    const char *texts[4] = {
        "The quick brown fox jumps over the lazy dog.",
        "A bird in the hand is worth two in the bush.",
        "Don't count your chickens before they hatch.",
        "The grass is always greener on the other side."
    };
    const int lines = 4;

    int height = LINES - 6;
    int width = COLS - 10;
    int starty = 3;
    int startx = 5;
    char title[128];
    snprintf(title, sizeof(title), "%s (Enter answer; q to quit)", m->name);
    WINDOW *win = tui_common_create_box(height, width, starty, startx, title);
    keypad(win, TRUE);

    bool use_colors = has_colors();
    if (use_colors) {
        start_color();
        /* red for wrong, yellow for correct on black background */
        init_pair(10, COLOR_RED, COLOR_BLACK);
        init_pair(11, COLOR_YELLOW, COLOR_BLACK);
    }

    char inputs[4][256];
    int pos[4] = {0,0,0,0};
    memset(inputs, 0, sizeof(inputs));

    int total_typed = 0;
    int total_correct = 0;
    time_t tstart = 0, tend = 0;
    int cur = 0;

    while (1) {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 1, 2, "%s - line %d of %d", m->name, cur+1, lines);
        mvwprintw(win, height - 3, 2, "Backspace allowed. Enter -> next line. Esc -> cancel.");

        /* draw each text, then an empty row, then input row, then an empty row */
        for (int i = 0; i < lines; ++i) {
            int text_row = 3 + i * 4;      /* text */
            int inrow = text_row + 2;     /* input (one blank line between) */
            mvwprintw(win, text_row, 4, "%s", texts[i]);

            int tlen = (int)strlen(texts[i]);
            /* draw typed characters and highlight incorrect ones */
            for (int j = 0; j < pos[i]; ++j) {
                char ch = inputs[i][j];
                char expected = (j < tlen) ? texts[i][j] : '\0';
                if (ch == expected) {
                    if (use_colors) {
                        wattron(win, COLOR_PAIR(11) | A_BOLD); /* yellow for correct */
                        mvwaddch(win, inrow, 4 + j, ch);
                        wattroff(win, COLOR_PAIR(11) | A_BOLD);
                    } else {
                        mvwaddch(win, inrow, 4 + j, ch);
                    }
                } else {
                    if (use_colors) {
                        wattron(win, COLOR_PAIR(10) | A_BOLD); /* red for wrong */
                        mvwaddch(win, inrow, 4 + j, ch);
                        wattroff(win, COLOR_PAIR(10) | A_BOLD);
                    } else {
                        wattron(win, A_REVERSE);
                        mvwaddch(win, inrow, 4 + j, ch);
                        wattroff(win, A_REVERSE);
                    }
                }
            }
            /* clear remainder of input area up to target length */
            for (int k = pos[i]; k < tlen; ++k) {
                mvwaddch(win, inrow, 4 + k, ' ');
            }

            /* place cursor on current input line */
            if (i == cur) {
                wmove(win, inrow, 4 + pos[i]);
            }
        }

        wrefresh(win);

        int ch = wgetch(win);
        if (!tstart && ch != ERR && ch != '\n' && ch != '\r') {
            tstart = time(NULL);
        }

        if (ch == '\n' || ch == '\r') {
            /* finish current line and move to next */
            cur++;
            if (cur >= lines) {
                /* finished all lines -> compute summary, mark complete and exit */
                tend = time(NULL);
                double elapsed = difftime(tend, tstart);
                if (elapsed <= 0.0) elapsed = 1.0;

                for (int i = 0; i < lines; ++i) {
                    int typed = pos[i];
                    total_typed += typed;
                    int tlen = (int)strlen(texts[i]);
                    for (int j = 0; j < typed; ++j) {
                        if (j < tlen && inputs[i][j] == texts[i][j]) total_correct++;
                    }
                }

                double accuracy = total_typed > 0 ? (100.0 * (double)total_correct / (double)total_typed) : 0.0;
                double wpm = (double)total_correct / 5.0 / (elapsed / 60.0);

                /* append leaderboard with accuracy */
                append_typing_leaderboard(user->name, m->id, wpm, accuracy);

                if (mission_complete(user->name, m->id)) {
                    tui_ncurses_toast("Mission complete! Reward granted", 900);
                    mission_load_user(user->name, user);
                } else {
                    tui_ncurses_toast("Failed to mark mission complete", 900);
                }

                /* show final summary and wait for Enter (or q/Esc) to return */
                werase(win);
                box(win, 0, 0);
                mvwprintw(win, 2, 4, "%s Complete!", m->name);
                mvwprintw(win, 4, 4, "Time: %.2f sec", elapsed);
                mvwprintw(win, 5, 4, "Accuracy: %.2f%% (%d/%d)", accuracy, total_correct, total_typed);
                mvwprintw(win, 6, 4, "WPM: %.2f", wpm);

                mvwprintw(win, 8, 4, "Leaderboard (100%% accuracy only, sorted by WPM):");

                /* read leaderboard, filter 100% accuracy and sort by WPM desc */
                typing_lbent entries[256];
                int n = 0;
                FILE *fp = fopen("data/typing_leaderboard.csv", "r");
                if (fp) {
                    char linebuf[256];
                    while (fgets(linebuf, sizeof(linebuf), fp) && n < (int)(sizeof(entries)/sizeof(entries[0]))) {
                        char uname[128]; int mid = 0; double w = 0, a = 0;
                        if (sscanf(linebuf, "%127[^,],%d,%lf,%lf", uname, &mid, &w, &a) == 4) {
                            if (mid == m->id && a >= 99.999) { /* treat >=100 as 100% */
                                strncpy(entries[n].name, uname, sizeof(entries[n].name)-1);
                                entries[n].name[sizeof(entries[n].name)-1] = '\0';
                                entries[n].wpm = w;
                                entries[n].acc = a;
                                n++;
                            }
                        }
                    }
                    fclose(fp);
                }
                /* sort by wpm desc */
                if (n > 1) {
                    qsort(entries, n, sizeof(entries[0]), cmp_typing_wpm_desc);
                }

                int row = 10;
                int shown = 0;
                for (int i = 0; i < n && row < height - 3 && shown < 5; ++i) {
                    mvwprintw(win, row++, 6, "%s : %.2f WPM", entries[i].name, entries[i].wpm);
                    shown++;
                }
                if (shown == 0) {
                    mvwprintw(win, 10, 6, "(no 100%% accuracy entries yet)");
                }

                mvwprintw(win, height - 3, 2, "Finished. Press Enter to return (Esc/q also exits).");
                wrefresh(win);
                /* wait for Enter / Esc / q */
                int k;
                while ((k = wgetch(win)) != '\n' && k != '\r' && k != 27 && k != 'q' && k != 'Q') {
                    /* looping until expected key */
                }

                tui_common_destroy_box(win);
                return;
            }
            continue;
        } else if (ch == 27) { /* Esc to cancel */
            tui_common_destroy_box(win);
            return;
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (pos[cur] > 0) {
                pos[cur]--;
                inputs[cur][pos[cur]] = '\0';
            }
        } else if (isprint(ch)) {
            int tlen = (int)strlen(texts[cur]);
            if (pos[cur] < tlen && pos[cur] < (int)sizeof(inputs[cur]) - 1) {
                inputs[cur][pos[cur]++] = (char)ch;
                inputs[cur][pos[cur]] = '\0';
            }
        }
    }

    tui_common_destroy_box(win);
}

/* Math quiz:
   - Generate problems up to 7 correct answers.
   - Problem types: +, -, * with constraints:
     + and -: operands up to 3 digits (0..999)
     *: operands up to 2 digits (0..99)
   - Show one problem at a time. User inputs answer and presses Enter.
   - On correct, move to next. After 7 correct, compute total time, append leaderboard.
   - On summary Enter, mark mission complete and exit.
*/
static int make_random_int(int max_plus_one) {
    return rand() % max_plus_one;
}

static void handle_mission_play_math(User *user, Mission *m) {
    if (!user || !m) return;
    srand((unsigned int)time(NULL) ^ (unsigned int)GETPID());

    int required = 7;
    int solved = 0;
    time_t tstart = 0, tend = 0;

    int height = LINES - 6;
    int width = COLS - 10;
    int starty = 3;
    int startx = 5;
    char title[128];
    snprintf(title, sizeof(title), "%s (Enter answer; q to quit)", m->name);
    WINDOW *win = tui_common_create_box(height, width, starty, startx, title);
    keypad(win, TRUE);

    while (solved < required) {
        /* generate a random problem */
        int typ = rand() % 3; /* 0:+ 1:- 2:* */
        int a, b;
        long correct = 0;
        if (typ == 0) { /* + up to 3 digits */
            a = make_random_int(1000);
            b = make_random_int(1000);
            correct = (long)a + b;
        } else if (typ == 1) { /* - up to 3 digits */
            a = make_random_int(1000);
            b = make_random_int(1000);
            correct = (long)a - b;
        } else { /* * up to 2 digits */
            a = make_random_int(100);
            b = make_random_int(100);
            correct = (long)a * b;
        }

        char prompt[128];
        snprintf(prompt, sizeof(prompt), "%d %c %d = ", a, (typ == 0 ? '+' : typ == 1 ? '-' : '*'), b);

        char ibuf[64];
        memset(ibuf, 0, sizeof(ibuf));
        int ipos = 0;
        int started = 0;

        while (1) {
            werase(win);
            box(win, 0, 0);
            mvwprintw(win, 2, 4, "%s: %d / %d", m->name, solved, required);
            mvwprintw(win, 4, 4, "%s", prompt);
            mvwprintw(win, 4, 4 + (int)strlen(prompt), "%s", ibuf);
            mvwprintw(win, height - 3, 2, "Enter numeric answer and press Enter. q to cancel.");
            wrefresh(win);

            int ch = wgetch(win);
            if (!started && ch != ERR) {
                tstart = tstart ? tstart : time(NULL);
                started = 1;
            }
            if (ch == '\n' || ch == '\r') {
                /* parse answer */
                long ans = strtol(ibuf, NULL, 10);
                if (ans == correct) {
                    solved++;
                    break;
                } else {
                    /* feedback and allow retry for same problem */
                    mvwprintw(win, height - 4, 4, "Incorrect, try again.");
                    wrefresh(win);
                    napms(800);
                    ipos = 0;
                    ibuf[0] = '\0';
                    continue;
                }
            } else if (ch == 'q' || ch == 27) {
                tui_common_destroy_box(win);
                return;
            } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
                if (ipos > 0) ipos--;
                ibuf[ipos] = '\0';
            } else if ((ch == '-' && ipos == 0) || isdigit(ch)) {
                if (ipos < (int)sizeof(ibuf) - 1) {
                    ibuf[ipos++] = (char)ch;
                    ibuf[ipos] = '\0';
                }
            }
        }
    }

    tend = time(NULL);
    double total_seconds = difftime(tend, tstart);
    if (total_seconds < 0.001) total_seconds = 1.0;

    /* show summary and leaderboard */
    werase(win);
    box(win, 0, 0);
    mvwprintw(win, 2, 4, "%s Complete!", m->name);
    mvwprintw(win, 4, 4, "Total time: %.2f sec for %d problems", total_seconds, required);
    append_math_leaderboard(user->name, m->id, total_seconds);

    /* print recent entries for this mission */
    typedef struct { char name[128]; double time; } mentry;
    mentry entries[256];
    int nents = 0;
    FILE *fp = fopen("data/math_leaderboard.csv", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp) && nents < (int)(sizeof(entries)/sizeof(entries[0]))) {
            char uname[128]; int mid = 0; double t = 0;
            if (sscanf(line, "%127[^,],%d,%lf", uname, &mid, &t) == 3) {
                if (mid == m->id) {
                    strncpy(entries[nents].name, uname, sizeof(entries[nents].name)-1);
                    entries[nents].name[sizeof(entries[nents].name)-1] = '\0';
                    entries[nents].time = t;
                    nents++;
                }
            }
        }
        fclose(fp);
    }
    /* sort ascending by time and display top (fastest) entries */
    if (nents > 1) {
        qsort(entries, nents, sizeof(entries[0]), cmp_math_time);
    }
    int row = 6;
    int shown = 0;
    for (int i = 0; i < nents && row < height - 2 && shown < 5; ++i) {
        mvwprintw(win, row++, 6, "%s : %.3f s", entries[i].name, entries[i].time);
        shown++;
    }
    if (shown == 0) {
        mvwprintw(win, 6, 6, "(no entries yet)");
    }

    mvwprintw(win, height - 3, 2, "Press Enter to complete mission and exit.");
    wrefresh(win);
    wgetch(win);

    if (mission_complete(user->name, m->id)) {
        tui_ncurses_toast("Mission complete! Reward granted", 900);
        mission_load_user(user->name, user);
    } else {
        tui_ncurses_toast("Failed to mark mission complete", 900);
    }

    tui_common_destroy_box(win);
}

/* integrate into mission board: when user presses Enter on a mission that is not completed,
   launch the correct play screen based on m->type */


static int get_owned_qty(User *user, const char *symbol) {
    if (!user || !symbol) return 0;

    for (int i = 0; i < user->holding_count; ++i) {
        if (strncmp(user->holdings[i].symbol,
                    symbol,
                    sizeof(user->holdings[i].symbol)) == 0) {
            return user->holdings[i].qty;
        }
    }
    return 0;
}

/* 선택한 한 종목의 log[] 전체를 그래프(@)로 보여주는 화면 */
/* log 길이가 화면보다 길면 ← / → 로 스크롤 가능 */
static void handle_stock_graph_view(const Stock *stock) {
    if (!stock) {
        return;
    }
    if (stock->log_len <= 0) {
        tui_ncurses_toast("No history for this stock", 800);
        return;
    }

    int height = LINES - 4;
    int width  = COLS  - 6;
    if (height < 10) height = LINES;  // 너무 작으면 대충 커버
    if (width  < 30) width  = COLS;

    char title[80];
    snprintf(title, sizeof(title), "Graph - %s (log size: %d)", stock->name, stock->log_len);

    WINDOW *win = tui_common_create_box(
        height,
        width,
        2,
        3,
        title
    );
    if (!win) return;

    keypad(win, TRUE);

    int offset  = 0;   // log[]에서 시작 인덱스
    int running = 1;

    while (running) {
        werase(win);
        box(win, 0, 0);

        /* 상단 정보 */
        mvwprintw(win, 0, 2, " %s Graph | points=%d ",
                  stock->name, stock->log_len);

        /* 그래프 그릴 영역 설정 */
        int plot_top    = 2;
        int plot_bottom = height - 3;        // 아래 한 줄은 안내용
        int plot_left   = 5;
        int plot_right  = width - 3;

        int plot_height = plot_bottom - plot_top + 1;
        int plot_width  = plot_right - plot_left + 1;

        if (plot_height < 3) plot_height = 3;
        if (plot_width  < 5) plot_width  = 5;

        int len = stock->log_len;

        /* offset 범위 정리 */
        if (offset < 0) offset = 0;
        if (offset > len - 1) offset = len - 1;
        if (len <= plot_width) {
            offset = 0;
        } else {
            if (offset + plot_width > len) {
                offset = len - plot_width;
            }
        }

        /* 현재 화면에 보여줄 구간의 min/max 찾기 */
        int window_end = offset + plot_width;
        if (window_end > len) window_end = len;

        int minv = stock->log[offset];
        int maxv = stock->log[offset];
        for (int i = offset + 1; i < window_end; ++i) {
            if (stock->log[i] < minv) minv = stock->log[i];
            if (stock->log[i] > maxv) maxv = stock->log[i];
        }
        if (maxv == minv) {
            /* 모두 같은 값이면, 수직 크기 1이라도 나오게 보정 */
            maxv = minv + 1;
        }

        /* Y축 눈금 정보 (좌측에 min/max 표시) */
        mvwprintw(win, plot_top,   1, "%d", maxv);
        mvwprintw(win, plot_bottom,1, "%d", minv);

        /* 실제 그래프 그리기: 각 x(열)마다 수직 바를 @로 찍는다 */
        for (int x = 0; x < plot_width; ++x) {
            int idx = offset + x;
            if (idx >= len) break;

            int v = stock->log[idx];

            double ratio = (double)(v - minv) / (double)(maxv - minv);
            if (ratio < 0.0) ratio = 0.0;
            if (ratio > 1.0) ratio = 1.0;

            int bar_h = (int)(ratio * (plot_height - 1)) + 1; // 최소 1칸은 찍히게
            if (bar_h > plot_height) bar_h = plot_height;

            
            int screen_x = plot_left + x;
            /* --- 스무스 라인 그래프: 점과 점 사이 채우기 --- */
            int prev_x = -1;
            int prev_y = -1;

            for (int x = 0; x < plot_width; ++x) {
                int idx = offset + x;
                if (idx >= len) break;

                int v = stock->log[idx];
                double ratio = (double)(v - minv) / (double)(maxv - minv);
                if (ratio < 0) ratio = 0;
                if (ratio > 1) ratio = 1;

                int bar_h = (int)(ratio * (plot_height - 1)) + 1;
                if (bar_h > plot_height) bar_h = plot_height;

                int y = plot_bottom - (bar_h - 1);
                if (y < plot_top) y = plot_top;

                int sx = plot_left + x;

                /* 현재 점 찍기 */
                mvwaddch(win, y, sx, '*');

                /* 이전 점과 연결해주기 */
                if (prev_x >= 0) {
                    int dx = sx - prev_x;
                    int dy = y  - prev_y;

                    int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
                    if (steps == 0) steps = 1;

                    double stepx = dx / (double)steps;
                    double stepy = dy / (double)steps;

                    double cx = prev_x;
                    double cy = prev_y;

                    for (int s = 1; s < steps; ++s) {
                        cx += stepx;
                        cy += stepy;
                        mvwaddch(win, (int)(cy + 0.5), (int)(cx + 0.5), '*');
                    }
                }

                prev_x = sx;
                prev_y = y;
}
        }
            // 막대그래프
        //     for (int k = 0; k < bar_h; ++k) {
        //         int y = plot_bottom - k;
        //         if (y < plot_top) break; 
        //         mvwaddch(win, y, screen_x, '@');
        //     }
        // }


        /* 아래쪽 안내 & 현재 구간 표시 */
        mvwprintw(win, height - 2, 2,
                  "←/→ scroll  q back  range [%d - %d] / %d",
                  offset, window_end - 1, len);

        wrefresh(win);

        int ch = wgetch(win);
        if (ch == KEY_LEFT) {
            /* 왼쪽으로 plot_width/2 만큼 스크롤 */
            int step = plot_width / 2;
            if (step < 1) step = 1;
            offset -= step;
            if (offset < 0) offset = 0;
        } else if (ch == KEY_RIGHT) {
            int step = plot_width / 2;
            if (step < 1) step = 1;
            offset += step;
            if (offset > len - 1) offset = len - 1;
        } else if (ch == 'q' || ch == 27) {
            running = 0;
        }
    }

    tui_common_destroy_box(win);
}

static void handle_stocks_view(User *user) {
    Stock stocks[16];
    int count = 0;

    /* 들어올 때 한 번 시간 업데이트 */
    stock_maybe_update_by_time();
    if (!stock_list(stocks, &count) || count == 0) {
        tui_ncurses_toast("No stock data", 800);
        return;
    }

    int height = LINES - 4;
    int width  = COLS  - 6;

    WINDOW *win = tui_common_create_box(
        height,
        width,
        2,
        3,
        "Stocks (Enter=Buy / s=Sell / d=Dividend / g=Graph / q=Close)"
    );
    if (!win) return;

    keypad(win, TRUE);
    int highlight = 0;
    int running   = 1;

    while (running) {
        /* 1시간 지났으면 내부에서 주가 변경 (CSV는 안 건드림) */
        stock_maybe_update_by_time();
        stock_list(stocks, &count);

        werase(win);
        box(win, 0, 0);

        /* 상단 타이틀 + 잔액 표시 */
        mvwprintw(win, 0, 2,
                  " Stocks - Balance %dCr ",
                  user->bank.balance);

        /* 헤더 */
        mvwprintw(win, 1, 2,
                  "%-3s %-8s %-7s %-5s %-6s %-20s",
                  "ID", "NAME", "PRICE", "OWN", "diff", "NEWS");

        int visible_rows = height - 4; // 위에 2줄 + 아래 안내 한 줄 빼고

        for (int i = 0; i < count && i < visible_rows; ++i) {
            int row    = 2 + i;
            int owned  = get_owned_qty(user, stocks[i].name);
            int diff   = stocks[i].current_price - stocks[i].previous_price;
            char diff_str[8];

            if (stocks[i].previous_price == 0) {
                snprintf(diff_str, sizeof(diff_str), " - ");
            } else if (diff > 0) {
                snprintf(diff_str, sizeof(diff_str), "+%d", diff);
            } else if (diff < 0) {
                snprintf(diff_str, sizeof(diff_str), "%d", diff);
            } else {
                snprintf(diff_str, sizeof(diff_str), "0");
            }

            if (i == highlight) {
                wattron(win, A_REVERSE);
            }

            mvwprintw(
                win,
                row,
                2,
                "%-3d %-8s %-7d %-5d %-4s %-20s",
                stocks[i].id,
                stocks[i].name,
                stocks[i].current_price,
                owned,
                diff_str,
                stocks[i].news
            );

            if (i == highlight) {
                wattroff(win, A_REVERSE);
            }
        }

        /* 아래쪽 조작 안내 (여기를 '버튼' 느낌으로 써도 됨) */
        mvwprintw(win, height - 2, 2,
                  "↑↓ move  Enter buy  s sell  g graph  q close");

        wrefresh(win);

        int ch = wgetch(win);

        if (ch == KEY_UP) {
            if (count > 0) {
                highlight = (highlight - 1 + count) % count;
            }
        } else if (ch == KEY_DOWN) {
            if (count > 0) {
                highlight = (highlight + 1) % count;
            }
        } else if (ch == '\n' || ch == '\r') {
            /* 매수: 선택 종목 1주 */
            if (count <= 0) continue;
            Stock *s = &stocks[highlight];

            if (stock_deal(user->name, s->name, 1, 1)) {
                tui_ncurses_toast("Buy complete", 800);
                /* stock_maybe_update_by_time() + stock_list()에서 가격 갱신 */
            } else {
                tui_ncurses_toast("Buy failed", 800);
            }
        } else if (ch == 's' || ch == 'S') {
            /* 매도: 선택 종목 1주 */
            if (count <= 0) continue;
            Stock *s = &stocks[highlight];

            if (stock_deal(user->name, s->name, 1, 0)) {
                tui_ncurses_toast("Sell complete", 800);
            } else {
                tui_ncurses_toast("Not enough shares", 800);
            }
        } else if (ch == 'g' || ch == 'G') {
            /* 🔹 현재 선택된 종목의 그래프 화면으로 진입 */
            if (count > 0) {
                Stock s = stocks[highlight];       // 복사해서 넘김(log 포함)
                handle_stock_graph_view(&s);
                /* 돌아오면 다시 while 루프 계속 → 리스트 화면 유지 */
            }
        } else if (ch == 'q' || ch == 27) {
            running = 0;
        }
    }

    tui_common_destroy_box(win);
}