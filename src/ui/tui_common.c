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

void tui_common_destroy_box(WINDOW *win) {
    if (!win) {
        return;
    }
    delwin(win);
}

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

void tui_common_draw_progress(WINDOW *win, int row, int col, int width, int percent) {
    if (!win || width <= 0) {
        return;
    }
    int filled = percent * width / 100;
    for (int i = 0; i < width; ++i) {
        mvwaddch(win, row, col + i, i < filled ? '#' : '-');
    }
    mvwprintw(win, row, col + width + 1, "%3d%%", percent);
    wrefresh(win);
}

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

void tui_common_print_multiline(WINDOW *win, int row, int col, const char *const *lines, size_t line_count) {
    if (!win || !lines) {
        return;
    }
    for (size_t i = 0; i < line_count; ++i) {
        mvwprintw(win, row + (int)i, col, "%s", lines[i]);
    }
    wrefresh(win);
}
