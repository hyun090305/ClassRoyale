/*
 * 파일 목적: tui_common UI 헤더 정의
 * 작성자: ChatGPT
 * 작성일: 2024-06-13
 * 수정 이력: 2024-06-13 ChatGPT - 주석 규칙 적용
 */
#ifndef UI_TUI_COMMON_H
#define UI_TUI_COMMON_H

#include <ncurses.h>
#include <stddef.h>

WINDOW *tui_common_create_box(int height, int width, int y, int x, const char *title);
void tui_common_destroy_box(WINDOW *win);
void tui_common_draw_menu(WINDOW *win, const char **entries, int entry_count, int highlight);
void tui_common_draw_progress(WINDOW *win, int row, int col, int width, int percent);
void tui_common_draw_help(const char *text);
void tui_common_print_multiline(WINDOW *win, int row, int col, const char *const *lines, size_t line_count);

#endif /* UI_TUI_COMMON_H */
