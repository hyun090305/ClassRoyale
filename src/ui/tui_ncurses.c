/*
 * 파일 목적: tui_ncurses 관련 터미널 UI 로직 구현
 * 작성자: ChatGPT
 * 작성일: 2024-06-13
 * 수정 이력: 2024-06-13 ChatGPT - 주석 규칙 적용
 */
#include "../../include/ui/tui_ncurses.h"

#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static WINDOW *g_main_window = NULL;

/* 함수 목적: tui_ncurses_init 함수는 tui_ncurses 관련 터미널 UI 로직 구현에서 필요한 동작을 수행합니다.
 * 매개변수: 없음
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
void tui_ncurses_init(void) {
    if (g_main_window) {
        return;
    }
    setlocale(LC_ALL, "");
    g_main_window = initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    nodelay(stdscr, FALSE);
    start_color();
    use_default_colors();
    init_pair(CP_DEFAULT, COLOR_CYAN, -1);
    wbkgd(stdscr, COLOR_PAIR(CP_DEFAULT));
    refresh();
}

/* 함수 목적: tui_ncurses_shutdown 함수는 tui_ncurses 관련 터미널 UI 로직 구현에서 필요한 동작을 수행합니다.
 * 매개변수: 없음
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
void tui_ncurses_shutdown(void) {
    if (!g_main_window) {
        return;
    }
    endwin();
    g_main_window = NULL;
}

/* 함수 목적: tui_ncurses_draw_logo 함수는 tui_ncurses 관련 터미널 UI 로직 구현에서 필요한 동작을 수행합니다.
 * 매개변수: win, y, x
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
void tui_ncurses_draw_logo(WINDOW *win, int y, int x) {
    if (!win) {

        
        win = stdscr;
    }
    const char *logo[] = {

"       _                                              _       ",
"      | |                                            | |      ",
"  ___ | |  __ _  ___  ___   ___   ___   _   _   __ _ | |  ___",
" / __|| | / _` |/ __|/ __| |  _| / _ \\ | | | | / _` || | / _ \\",
"| (__ | || (_| |\\__ \\\\__ \\ | |  | (_) || |_| || (_| || ||  __/",
" \\___||_| \\__,_||___/|___/ |_|   \\___/  \\__, | \\__,_||_| \\___|",
"                                         __/ |                ",
"                                        |___/                 ",

    };
    for (size_t i = 0; i < sizeof(logo) / sizeof(logo[0]); ++i) {
        mvwprintw(win, y + 4 + (int)i, x - 3 , "%s", logo[i]);
    }
    wrefresh(win);
}

/* 함수 목적: tui_ncurses_draw_status 함수는 tui_ncurses 관련 터미널 UI 로직 구현에서 필요한 동작을 수행합니다.
 * 매개변수: message
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
void tui_ncurses_draw_status(const char *message) {
    if (!message) {
        message = "";
    }
    int row = LINES - 2;
    attron(A_REVERSE);
    mvhline(row, 0, ' ', COLS);
    mvprintw(row, 2, "%s", message);
    attroff(A_REVERSE);
    refresh();
}

/* 함수 목적: tui_ncurses_prompt_line 함수는 tui_ncurses 관련 터미널 UI 로직 구현에서 필요한 동작을 수행합니다.
 * 매개변수: win, row, col, label, buffer, len, hidden
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
int tui_ncurses_prompt_line(WINDOW *win, int row, int col, const char *label, char *buffer, size_t len, int hidden) {
    if (!win || !buffer || len == 0) {
        return 0;
    }
    int cur_col = col;
    if (label) {
        mvwprintw(win, row, col, "%s: ", label);
        cur_col += (int)strlen(label) + 2;
    }
    wmove(win, row, cur_col);
    //wclrtoeol(win);
    wrefresh(win);
    noecho();
    curs_set(1);
    int start_index = 0;
    if (buffer[0] != '\0') {
        start_index = (int)strlen(buffer);
        if (start_index > (int)len - 1) {
            start_index = (int)len - 1;
            buffer[start_index] = '\0';
        }
        /* 화면에 미리 채워진 내용 출력 */
        mvwprintw(win, row, cur_col, "%s", buffer);
        wmove(win, row, cur_col + start_index);
        wrefresh(win);
    }

    int index = 0;
    int ch;
    while ((ch = wgetch(win)) != '\n' && ch != '\r') {
        if (ch == KEY_BACKSPACE || ch == 127) {
            if (index > start_index) {
                index--;
                buffer[index] = '\0';
                mvwaddch(win, row, cur_col + index, ' ');
                wmove(win, row, cur_col + index);
                wrefresh(win);
            }
            continue;
        }
        if (ch == 27) {
            buffer[0] = '\0';
            noecho();
            curs_set(0);
            return 0;
        }
        if (index < (int)len - 1 && ch >= 32 && ch < 127) {
            buffer[index++] = (char)ch;
            buffer[index] = '\0';
            if (hidden) {
                mvwaddch(win, row, cur_col + index - 1, '*');
            } else {
                mvwaddch(win, row, cur_col + index - 1, ch);
            }
            wrefresh(win);
        }
    }
    buffer[index] = '\0';
    noecho();
    curs_set(0);
    return 1;
}

/* 함수 목적: tui_ncurses_prompt_number 함수는 tui_ncurses 관련 터미널 UI 로직 구현에서 필요한 동작을 수행합니다.
 * 매개변수: context, label, out_value
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
int tui_ncurses_prompt_number(WINDOW *context, const char *label, int *out_value) {
    if (!out_value) {
        return 0;
    }
    char tmp[32];
    int row = getmaxy(context) - 2;
    if (row < 1) {
        row = 1;
    }
    if (!tui_ncurses_prompt_line(context, row, 2, label, tmp, sizeof(tmp), 0)) {
        return 0;
    }
    *out_value = atoi(tmp);
    return 1;
}

/* 함수 목적: tui_ncurses_toast 함수는 tui_ncurses 관련 터미널 UI 로직 구현에서 필요한 동작을 수행합니다.
 * 매개변수: message, delay_ms
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
void tui_ncurses_toast(const char *message, int delay_ms) {
    tui_ncurses_draw_status(message);
    if (delay_ms > 0) {
        napms(delay_ms);
    }
}
