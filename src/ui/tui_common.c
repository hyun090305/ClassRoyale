/*
 * 파일 목적: tui_common 관련 터미널 UI 로직 구현
 * 작성자: ChatGPT
 * 작성일: 2024-06-13
 * 수정 이력: 2024-06-13 ChatGPT - 주석 규칙 적용
 */
#include "../../include/ui/tui_common.h"

#include <string.h>

WINDOW *tui_common_create_box(int height, int width, int y, int x, const char *title) {
    WINDOW *win = newwin(height, width, y, x);
    keypad(win, TRUE);
    box(win, 0, 0);
    if (title) {
        wattron(win, A_BOLD);
        mvwprintw(win, 0, 2, " %s ", title);
        wattroff(win, A_BOLD);
    }
    wrefresh(win);
    return win;
}

/* 함수 목적: tui_common_destroy_box 함수는 tui_common 관련 터미널 UI 로직 구현에서 필요한 동작을 수행합니다.
 * 매개변수: win
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
void tui_common_destroy_box(WINDOW *win) {
    if (!win) {
        return;
    }
    /* Before deleting the window, copy its contents into stdscr
     * so that subsequent calls to refresh() (which redraw stdscr)
     * do not erase the visual contents produced by the box. We
     * use copywin to map the window region onto the correct
     * coordinates of stdscr. */
    int wy, wx, h, w;
    getbegyx(win, wy, wx);
    getmaxyx(win, h, w);
    /* copywin(src, dest, sminrow, smincol, dminrow, dmincol, dmaxrow, dmaxcol, overlay) */
    copywin(win, stdscr, 0, 0, wy, wx, wy + h - 1, wx + w - 1, 0);
    delwin(win);
}

/* 함수 목적: tui_common_draw_menu 함수는 tui_common 관련 터미널 UI 로직 구현에서 필요한 동작을 수행합니다.
 * 매개변수: win, entries, entry_count, highlight
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
void tui_common_draw_menu(WINDOW *win, const char **entries, int entry_count, int highlight) {
    if (!win || !entries) {
        return;
    }
    for (int i = 0; i < entry_count; ++i) {
        if (i == highlight) {
            wattron(win, A_REVERSE);
        }
        mvwprintw(win, 2 + i, 2, "%s", entries[i]);
        if (i == highlight) {
            wattroff(win, A_REVERSE);
        }
    }
    wrefresh(win);
}

/* 함수 목적: tui_common_draw_progress 함수는 tui_common 관련 터미널 UI 로직 구현에서 필요한 동작을 수행합니다.
 * 매개변수: win, row, col, width, percent
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
void tui_common_draw_progress(WINDOW *win, int row, int col, int width, int percent) {
    if (!win || width <= 0) {
        return;
    }
    int maxx = getmaxx(win);
    if (col < 0) col = 0;
    /* ensure there's room for the bar and the " 100%" text */
    if (col >= maxx) {
        return;
    }
    if (col + width + 5 > maxx) {
        width = maxx - col - 5;
    }
    if (width <= 0) {
        return;
    }
    int filled = percent * width / 100;
    for (int i = 0; i < width; ++i) {
        mvwaddch(win, row, col + i, i < filled ? '#' : '-');
    }
    mvwprintw(win, row, col + width + 1, "%3d%%", percent);
    wrefresh(win);
}

/* 함수 목적: tui_common_draw_help 함수는 tui_common 관련 터미널 UI 로직 구현에서 필요한 동작을 수행합니다.
 * 매개변수: text
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
void tui_common_draw_help(const char *text) {
    int row = LINES - 1;
    attron(A_REVERSE);
    mvhline(row, 0, ' ', COLS);
    if (text) {
        mvprintw(row, 2, "%s", text);
    }
    attroff(A_REVERSE);
    refresh();
}

/* 함수 목적: tui_common_print_multiline 함수는 tui_common 관련 터미널 UI 로직 구현에서 필요한 동작을 수행합니다.
 * 매개변수: win, row, col, lines, line_count
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
void tui_common_print_multiline(WINDOW *win, int row, int col, const char *const *lines, size_t line_count) {
    if (!win || !lines) {
        return;
    }
    for (size_t i = 0; i < line_count; ++i) {
        mvwprintw(win, row + (int)i, col, "%s", lines[i]);
    }
    wrefresh(win);
}
