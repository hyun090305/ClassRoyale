#include "../../include/ui/tui_teacher.h"

#include <stdlib.h>
#include <string.h>

#include "../../include/domain/account.h"
#include "../../include/domain/admin.h"
#include "../../include/domain/mission.h"
#include "../../include/domain/shop.h"
#include "../../include/domain/user.h"
#include "../../include/ui/tui_common.h"
#include "../../include/ui/tui_ncurses.h"

static int collect_students(User *out[], int max_items) {
    int count = 0;
    size_t total = user_count();
    for (size_t i = 0; i < total && count < max_items; ++i) {
        const User *entry = user_at(i);
        if (entry && entry->isadmin == STUDENT) {
            User *mutable_entry = user_lookup(entry->name);
            if (mutable_entry) {
                out[count++] = mutable_entry;
            }
        }
    }
    return count;
}

static void draw_teacher_dashboard(User *user, const char *status) {
    erase();
    mvprintw(1, (COLS - 32) / 2, "Class Royale - Teacher Dashboard");
    size_t total = user_count();
    int student_count = 0;
    long total_balance = 0;
    for (size_t i = 0; i < total; ++i) {
        const User *entry = user_at(i);
        if (!entry) {
            continue;
        }
        if (entry->isadmin == STUDENT) {
            student_count++;
        }
        total_balance += entry->bank.balance;
    }
    WINDOW *summary = tui_common_create_box(6, COLS - 4, 3, 2, "Class Summary");
    mvwprintw(summary, 1, 2, "Students: %d", student_count);
    mvwprintw(summary, 2, 2, "Total Currency: %ld Cr", total_balance);
    mvwprintw(summary, 3, 2, "Admin: %s", user->name);
    wrefresh(summary);
    tui_common_destroy_box(summary);

    Mission missions[10];
    int mission_count = 0;
    mission_list_open(missions, &mission_count);
    WINDOW *mission_win = tui_common_create_box(LINES - 12, (COLS / 2) - 3, 9, 2, "Mission Management");
    mvwprintw(mission_win, 1, 2, "Ongoing Missions");
    for (int i = 0; i < mission_count && i < getmaxy(mission_win) - 2; ++i) {
        mvwprintw(mission_win, 2 + i, 2, "#%d %s Reward %dCr", missions[i].id, missions[i].name, missions[i].reward);
    }
    if (mission_count == 0) {
        mvwprintw(mission_win, 2, 2, "No missions registered. Press 'm' to add a new mission.");
    }
    wrefresh(mission_win);
    tui_common_destroy_box(mission_win);

    Shop shops[2];
    int shop_count = 0;
    shop_list(shops, &shop_count);
    WINDOW *shop_win = tui_common_create_box(LINES - 12, (COLS / 2) - 3, 9, (COLS / 2) + 1, "Shop Statistics");
    if (shop_count > 0) {
        const Shop *shop = &shops[0];
        mvwprintw(shop_win, 1, 2, "%s Income: %dCr", shop->name, shop->income);
        for (int i = 0; i < shop->item_count && i < getmaxy(shop_win) - 3; ++i) {
            mvwprintw(shop_win, 2 + i, 2, "%s Sales:%d Stock:%d", shop->items[i].name, shop->sales[i], shop->items[i].stock);
        }
    } else {
        mvwprintw(shop_win, 1, 2, "No shop data");
    }
    wrefresh(shop_win);
    tui_common_destroy_box(shop_win);

    tui_common_draw_help("m:New mission s:Student management n:Message q:Logout");
    tui_ncurses_draw_status(status);
    refresh();
}

static void handle_new_mission(void) {
    int height = 12;
    int width = 60;
    WINDOW *win = tui_common_create_box(height, width, (LINES - height) / 2, (COLS - width) / 2, "Create New Mission");
    char title[64];
    char reward_buf[16];
    memset(title, 0, sizeof(title));
    memset(reward_buf, 0, sizeof(reward_buf));
    if (!tui_ncurses_prompt_line(win, 2, 2, "Title", title, sizeof(title), 0)) {
        tui_common_destroy_box(win);
        return;
    }
    if (!tui_ncurses_prompt_line(win, 3, 2, "Reward Cr", reward_buf, sizeof(reward_buf), 0)) {
        tui_common_destroy_box(win);
        return;
    }
    Mission m = {0};
    snprintf(m.name, sizeof(m.name), "%s", title);
    m.reward = atoi(reward_buf);
    m.type = 1;
    if (mission_create(&m)) {
        tui_ncurses_toast("New mission registered", 900);
    } else {
        tui_ncurses_toast("Mission registration failed", 900);
    }
    tui_common_destroy_box(win);
}

static void handle_broadcast(void) {
    int height = 8;
    int width = 60;
    WINDOW *win = tui_common_create_box(height, width, (LINES - height) / 2, (COLS - width) / 2, "Send Message");
    char message[100];
    if (tui_ncurses_prompt_line(win, 2, 2, "Message to send", message, sizeof(message), 0) && *message) {
        admin_message_all(message);
        tui_ncurses_toast("Notified students", 900);
    }
    tui_common_destroy_box(win);
}

static void handle_student_list(void) {
    User *students[MAX_STUDENTS];
    int count = collect_students(students, MAX_STUDENTS);
    if (count == 0) {
        tui_ncurses_toast("No student accounts", 800);
        return;
    }
    int height = LINES - 4;
    int width = COLS - 6;
    WINDOW *win = tui_common_create_box(height, width, 2, 3, "Student Management (+/- adjust balance, q to close)");
    int highlight = 0;
    keypad(win, TRUE);
    while (1) {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 0, 2, " Student Management (+/- adjust balance, q to close) ");
        for (int i = 0; i < count && i < height - 2; ++i) {
            if (i == highlight) {
                wattron(win, A_REVERSE);
            }
            User *entry = students[i];
            mvwprintw(win, 1 + i, 2, "%s Balance:%dCr Rating:%c Missions:%d/%d", entry->name, entry->bank.balance,
                      entry->bank.rating, entry->completed_missions, entry->total_missions);
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
        } else if (ch == '+' || ch == '=') {
            account_add_tx(students[highlight], 50, "ADMIN_GRANT");
            tui_ncurses_toast("+50Cr granted", 700);
        } else if (ch == '-' || ch == '_') {
            if (!account_add_tx(students[highlight], -50, "ADMIN_DEDUCT")) {
                tui_ncurses_toast("Deduction failed", 700);
            } else {
                tui_ncurses_toast("-50Cr deducted", 700);
            }
        } else if (ch == 'q' || ch == 27) {
            break;
        }
    }
    tui_common_destroy_box(win);
}

void tui_teacher_loop(User *user) {
    if (!user) {
        return;
    }
    /* Ensure blocking getch() behavior so dashboard doesn't redraw in a tight loop */
    nodelay(stdscr, FALSE);
    const char *status = "Shortcut Keys";
    int running = 1;
    while (running) {
        draw_teacher_dashboard(user, status);
        int ch = getch();
        switch (ch) {
            case 'm':
            case 'M':
                handle_new_mission();
                status = "Added a new mission";
                break;
            case 's':
            case 'S':
                handle_student_list();
                status = "Managed student balances";
                break;
            case 'n':
            case 'N':
                handle_broadcast();
                status = "Sent announcement message";
                break;
            case 'q':
            case 'Q':
                running = 0;
                break;
            default:
                status = "Available commands: m,s,n,q";
                break;
        }
    }
}
