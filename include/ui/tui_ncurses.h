#ifndef UI_TUI_NCURSES_H
#define UI_TUI_NCURSES_H

#include <ncurses.h>
#include <stddef.h>

#include "types.h"

void tui_ncurses_init(void);
void tui_ncurses_shutdown(void);
void tui_ncurses_draw_logo(WINDOW *win, int y, int x);
void tui_ncurses_draw_status(const char *message);
int tui_ncurses_prompt_line(WINDOW *win, int row, int col, const char *label, char *buffer, size_t len, int hidden);
int tui_ncurses_prompt_number(WINDOW *context, const char *label, int *out_value);
void tui_ncurses_toast(const char *message, int delay_ms);

#endif /* UI_TUI_NCURSES_H */
