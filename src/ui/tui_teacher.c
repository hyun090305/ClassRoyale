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
#include "../../include/core/csv.h"

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
        total_balance += (long)entry->bank.balance + (long)entry->bank.cash;
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

    tui_common_draw_help("m:New mission s:Student management n:Message d:Assign QOTD q:Logout");
    tui_ncurses_draw_status(status);
    refresh();
}

static void handle_new_mission(void) {
    int height = 14;
    int width = 64;
    WINDOW *win = tui_common_create_box(height, width, (LINES - height) / 2, (COLS - width) / 2, "Create New Mission");
    char title[64];
    char reward_buf[16];
    char type_buf[8];
    memset(title, 0, sizeof(title));
    memset(reward_buf, 0, sizeof(reward_buf));
    memset(type_buf, 0, sizeof(type_buf));

    if (!tui_ncurses_prompt_line(win, 2, 2, "Title", title, sizeof(title), 0)) {
        tui_common_destroy_box(win);
        return;
    }
    if (!tui_ncurses_prompt_line(win, 3, 2, "Reward Cr", reward_buf, sizeof(reward_buf), 0)) {
        tui_common_destroy_box(win);
        return;
    }
    /* prompt for type: 0 = Typing Practice, 1 = Math Quiz */
    if (!tui_ncurses_prompt_line(win, 4, 2, "Type (0:Typing Practice, 1:Math Quiz)", type_buf, sizeof(type_buf), 0)) {
        tui_common_destroy_box(win);
        return;
    }
    int mtype = atoi(type_buf);
    if (mtype != 0 && mtype != 1) {
        mtype = 1; /* default to Math Quiz if invalid */
    }

    Mission m = {0};
    snprintf(m.name, sizeof(m.name), "%s", title);
    m.reward = atoi(reward_buf);
    m.type = mtype;
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
    char message[100]={};
    if (tui_ncurses_prompt_line(win, 2, 2, "Message to send", message, sizeof(message), 0) && *message) {
        admin_message_all(message);
        tui_ncurses_toast("Notified students", 900);
    }
    tui_common_destroy_box(win);
}

static void handle_qotd_assign(void) {
    int height = 18;
    int width = 72;
    WINDOW *win = tui_common_create_box(height, width, (LINES - height) / 2, (COLS - width) / 2, "Assign QOTD");
    char name[64];
    char date[32];
    char problem[512];
    char ans1[128];
    char ans2[128];
    char ans3[128];
    char right_ans[128];
    
    while (1) {
        memset(name, 0, sizeof(name));
        memset(date, 0, sizeof(date));
        memset(problem, 0, sizeof(problem));
        memset(ans1, 0, sizeof(ans1));
        memset(ans2, 0, sizeof(ans2));
        memset(ans3, 0, sizeof(ans3));
        memset(right_ans, 0, sizeof(right_ans));

        if (!tui_ncurses_prompt_line(win, 2, 2, "QOTD Name", name, sizeof(name), 0)) { tui_common_destroy_box(win); return; }
        if (!tui_ncurses_prompt_line(win, 3, 2, "Date (YYYY-MM-DD)", date, sizeof(date), 0)) { tui_common_destroy_box(win); return; }
        if (!tui_ncurses_prompt_line(win, 4, 2, "Question", problem, sizeof(problem), 0)) { tui_common_destroy_box(win); return; }
        /* optional possible answers */
        tui_ncurses_prompt_line(win, 6, 2, "Answer option 1", ans1, sizeof(ans1), 0);
        tui_ncurses_prompt_line(win, 7, 2, "Answer option 2", ans2, sizeof(ans2), 0);
        tui_ncurses_prompt_line(win, 8, 2, "Answer option 3", ans3, sizeof(ans3), 0);
        if (!tui_ncurses_prompt_line(win, 10, 2, "Right answer (number)", right_ans, sizeof(right_ans), 0)) { tui_common_destroy_box(win); return; }

        int right_idx = atoi(right_ans);
        if (right_idx < 1 || right_idx > 3) {
            tui_ncurses_toast("Invalid right answer (must be 1 to 3)", 900);
            continue; /* restart prompts */
        }

        /* check for duplicate date in existing CSV (pipe-delimited single-line rows) */
        int duplicate = 0;
        FILE *rf = fopen("data/qotd_questions.csv", "r");
        if (rf) {
            char line[2048];
            while (fgets(line, sizeof(line), rf)) {
                size_t r = strlen(line);
                if (r == 0) continue;
                if (line[r-1] == '\n') line[r-1] = '\0';
                /* parse simple: fields separated by ',' */
                char *pc = line;
                char *flds[8] = {0};
                int fi = 0;
                char *tok = strtok(pc, ",");
                while (tok && fi < 7) { flds[fi++] = tok; tok = strtok(NULL, ","); }
                if (fi >= 2 && flds[1] && strcmp(flds[1], date) == 0) {
                    duplicate = 1;
                    break;
                }
            }
            fclose(rf);
        }

        if (duplicate) {
            tui_ncurses_toast("Invalid date: QOTD already exists for that date", 1200);
            /* clear any queued input so prompts restart cleanly */
            flushinp();
            /* visually clear the assign window so old text disappears */
            werase(win);
            box(win, 0, 0);
            mvwprintw(win, 0, 2, " Assign QOTD ");
            wrefresh(win);
            /* restart assign process */
            continue;
        }

        /* persist as single pipe-delimited row: name|date|question|right_index|opt1|opt2|opt3 */
        csv_ensure_dir("data");
        FILE *fp = fopen("data/qotd_questions.csv", "a");
        if (!fp) {
            tui_ncurses_toast("Failed to open QOTD file", 900);
            tui_common_destroy_box(win);
            return;
        }
        fprintf(fp, "%s,%s,%s,%d,%s,%s,%s\n", name, date, problem, right_idx, ans1, ans2, ans3);
        fclose(fp);

        tui_ncurses_toast("QOTD assigned and saved", 900);
        tui_common_destroy_box(win);
        return;
    }
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
    int scroll_offset = 0; /* track which student is at the top of the visible list */
    /* leave two extra rows free so content doesn't touch the box borders */
    int visible_rows = height - 5;
    keypad(win, TRUE);
    while (1) {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 0, 2, " Student Management (+/- adjust balance, q to close) ");

        /* table header */
        int header_row = 1;
        mvwprintw(win, header_row, 2, "%-20s %10s %8s %8s %12s", "Name", "Deposit", "Cash", "Loan", "Missions");
        mvwprintw(win, header_row + 1, 2, "-----------------------------------------------------------------------");

        scroll_offset = highlight - visible_rows + 1;
        
        /* draw visible students */
        /* clamp scroll_offset so it's always within valid range */
        if (scroll_offset < 0) scroll_offset = 0;
        if (scroll_offset > count - visible_rows) scroll_offset = count - visible_rows;
        if (scroll_offset < 0) scroll_offset = 0;
        for (int i = 0; i < visible_rows && scroll_offset + i < count; ++i) {
            int student_idx = scroll_offset + i;
            int row = header_row + 2 + i; /* leave room for header + separator */
            if (student_idx == highlight) {
                wattron(win, A_REVERSE);
            }
            User *entry = students[student_idx];
            mvwprintw(win, row, 2, "%-20.20s %10d %8d %8d %6d/%-6d",
                      entry->name,
                      entry->bank.balance,
                      entry->bank.cash,
                      entry->bank.loan,
                      entry->completed_missions,
                      entry->total_missions);
            if (student_idx == highlight) {
                wattroff(win, A_REVERSE);
            }
        }

        /* draw scroll bar on right side */
        int scrollbar_col = width - 2;
        int scrollbar_height = visible_rows;
        /* calculate thumb position and size */
        int thumb_size = (count > 0) ? ((visible_rows * visible_rows) / count) : visible_rows;
        if (thumb_size < 1) thumb_size = 1;
        int thumb_pos = (count > visible_rows) ? ((scroll_offset * (scrollbar_height - thumb_size)) / (count - visible_rows)) : 0;
        /* draw scrollbar track and thumb aligned with student rows (lower by header) */
        int scrollbar_start_row = header_row + 2; /* align with first student row */
        for (int i = 0; i < scrollbar_height; ++i) {
            if (i >= thumb_pos && i < thumb_pos + thumb_size) {
                mvwaddch(win, scrollbar_start_row + i, scrollbar_col, '#');
            } else {
                mvwaddch(win, scrollbar_start_row + i, scrollbar_col, '|');
            }
        }

        wrefresh(win);
        int ch = wgetch(win);
        if (ch == KEY_UP) {
            if (highlight > 0) {
                highlight--;
            }
         } else if (ch == KEY_DOWN) {
            if (highlight < count - 1) {
                highlight++;
            }
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
            case 'd':
            case 'D':
                handle_qotd_assign();
                status = "Assigned QOTD";
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
                status = "Available commands: m,s,n,d,q";
                break;
        }
    }
}
