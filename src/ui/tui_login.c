#include "../../include/ui/tui_login.h"

#include <stdlib.h>
#include <string.h>

#include "../../include/domain/user.h"
#include "../../include/ui/tui_common.h"
#include "../../include/ui/tui_ncurses.h"

static void draw_welcome(int highlight, const char *status_line) {
    erase();
    int width = 64;
    int height = 20;
    int starty = (LINES - height) / 2;
    int startx = (COLS - width) / 2;
    WINDOW *win = tui_common_create_box(height, width, starty, startx, "Class Royale");
    tui_ncurses_draw_logo(win, 2, 4);
    const char *menu[] = {"로그인", "회원가입", "종료"};
    tui_common_draw_menu(win, menu, 3, highlight);
    mvwprintw(win, 10, 2, "학생들이 즐겁게 경제를 학습하는 Class Royale");
    mvwprintw(win, height - 3, 2, "방향키 이동 · Enter 선택");
    if (status_line && *status_line) {
        mvwprintw(win, height - 4, 2, "%s", status_line);
    }
    wrefresh(win);
    tui_common_destroy_box(win);
    refresh();
}

static rank_t prompt_role(WINDOW *form) {
    const char *roles[] = {"학생", "교사"};
    int highlight = 0;
    while (1) {
        for (int i = 0; i < 2; ++i) {
            if (i == highlight) {
                wattron(form, A_REVERSE);
            }
            mvwprintw(form, 6 + i, 2, "(%c) %s", i == highlight ? '>' : ' ', roles[i]);
            if (i == highlight) {
                wattroff(form, A_REVERSE);
            }
        }
        mvwprintw(form, 5, 2, "역할 선택 (방향키·Enter)");
        wrefresh(form);
        int ch = wgetch(form);
        if (ch == KEY_UP || ch == KEY_LEFT) {
            highlight = (highlight + 1) % 2;
        } else if (ch == KEY_DOWN || ch == KEY_RIGHT) {
            highlight = (highlight + 1) % 2;
        } else if (ch == '\n' || ch == '\r') {
            return highlight == 0 ? STUDENT : TEACHER;
        } else if (ch == 27) {
            return STUDENT;
        }
    }
}

enum {
    NAME_FIELD_CAP = sizeof(((User *)0)->name),
    PW_FIELD_CAP = sizeof(((User *)0)->pw)
};

static User *prompt_login(void) {
    int width = 60;
    int height = 12;
    WINDOW *form = tui_common_create_box(height, width, (LINES - height) / 2, (COLS - width) / 2, "로그인");
    char username[NAME_FIELD_CAP];
    char password[PW_FIELD_CAP];
    memset(username, 0, sizeof(username));
    memset(password, 0, sizeof(password));
    mvwprintw(form, 2, 2, "이름과 비밀번호를 입력하세요");
    wrefresh(form);
    if (!tui_ncurses_prompt_line(form, 4, 2, "이름", username, sizeof(username), 0)) {
        tui_common_destroy_box(form);
        return NULL;
    }
    if (!tui_ncurses_prompt_line(form, 5, 2, "비밀번호", password, sizeof(password), 1)) {
        tui_common_destroy_box(form);
        return NULL;
    }
    if (!user_auth(username, password)) {
        tui_ncurses_toast("로그인 실패 - 정보를 확인하세요", 1000);
        tui_common_destroy_box(form);
        return NULL;
    }
    User *user = user_lookup(username);
    tui_common_destroy_box(form);
    return user;
}

static void prompt_register(void) {
    int width = 60;
    int height = 14;
    WINDOW *form = tui_common_create_box(height, width, (LINES - height) / 2, (COLS - width) / 2, "회원가입");
    char username[NAME_FIELD_CAP];
    char password[PW_FIELD_CAP];
    memset(username, 0, sizeof(username));
    memset(password, 0, sizeof(password));
    mvwprintw(form, 2, 2, "새 계정을 만들어 주세요");
    wrefresh(form);
    if (!tui_ncurses_prompt_line(form, 3, 2, "이름", username, sizeof(username), 0)) {
        tui_common_destroy_box(form);
        return;
    }
    if (!tui_ncurses_prompt_line(form, 4, 2, "비밀번호", password, sizeof(password), 1)) {
        tui_common_destroy_box(form);
        return;
    }
    rank_t role = prompt_role(form);
    User newbie = {0};
    snprintf(newbie.name, sizeof(newbie.name), "%s", username);
    snprintf(newbie.id, sizeof(newbie.id), "%s", username);
    snprintf(newbie.pw, sizeof(newbie.pw), "%s", password);
    newbie.isadmin = role;
    snprintf(newbie.bank.name, sizeof(newbie.bank.name), "%s", username);
    newbie.bank.balance = role == STUDENT ? 1000 : 5000;
    newbie.bank.rating = 'B';
    if (user_register(&newbie)) {
        tui_ncurses_toast("가입 완료! 이제 로그인하세요", 1200);
    } else {
        tui_ncurses_toast("가입 실패 - 중복 이름인지 확인", 1200);
    }
    tui_common_destroy_box(form);
}

User *tui_login_flow(int demo_mode) {
    if (demo_mode) {
        return user_lookup("student");
    }
    int selection = 0;
    const int menu_count = 3;
    const char *status = "";
    while (1) {
        draw_welcome(selection, status);
        int ch = getch();
        if (ch == KEY_UP) {
            selection = (selection - 1 + menu_count) % menu_count;
        } else if (ch == KEY_DOWN) {
            selection = (selection + 1) % menu_count;
        } else if (ch == '\n' || ch == '\r') {
            if (selection == 0) {
                User *user = prompt_login();
                if (user) {
                    return user;
                }
                status = "로그인 실패";
            } else if (selection == 1) {
                prompt_register();
                status = "회원가입 완료 시 다시 로그인";
            } else {
                return NULL;
            }
        } else if (ch == 'q' || ch == 'Q') {
            return NULL;
        }
    }
}
