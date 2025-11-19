#include "ui/tui_student.h"

#include <stdlib.h>
#include <string.h>

#include "domain/account.h"
#include "domain/admin.h"
#include "domain/auction.h"
#include "domain/economy.h"
#include "domain/mission.h"
#include "domain/shop.h"
#include "domain/stock.h"
#include "domain/user.h"
#include "ui/tui_auction.h"
#include "ui/tui_common.h"
#include "ui/tui_ncurses.h"
#include "ui/tui_stock.h"

static void ensure_student_seed(User *user) {
    if (!user || user->mission_count > 0) {
        return;
    }
    Mission open[8];
    int count = 0;
    if (mission_list_open(open, &count)) {
        for (int i = 0; i < count && i < 4; ++i) {
            admin_assign_mission(user->name, &open[i]);
        }
    }
}

static void render_mission_preview(WINDOW *win, const User *user) {
    mvwprintw(win, 1, 2, "총 %d개 중 %d개 완료", user->mission_count, user->completed_missions);
    int row = 2;
    for (int i = 0; i < user->mission_count && row < getmaxy(win) - 1; ++i, ++row) {
        const Mission *mission = &user->missions[i];
        mvwprintw(win, row, 2, "#%d %-12s [%s] +%dCr", mission->id, mission->name,
                  mission->completed ? "완료" : "진행", mission->reward);
    }
    if (user->mission_count == 0) {
        mvwprintw(win, row, 2, "할당된 미션이 없습니다.");
    }
    wrefresh(win);
}

static void render_shop_preview(WINDOW *win) {
    Shop shops[2];
    int count = 0;
    if (!shop_list(shops, &count) || count == 0) {
        mvwprintw(win, 1, 2, "상점 데이터가 없습니다");
        wrefresh(win);
        return;
    }
    const Shop *shop = &shops[0];
    mvwprintw(win, 1, 2, "%s - 오늘의 인기 상품", shop->name);
    int limit = getmaxy(win) - 2;
    for (int i = 0; i < shop->item_count && i < limit; ++i) {
        mvwprintw(win, 2 + i, 2, "%s %3dCr [재고:%2d]", shop->items[i].name, shop->items[i].cost,
                  shop->items[i].stock);
    }
    wrefresh(win);
}

static void render_news(WINDOW *win) {
    const char *lines[] = {
        "최근 알림",
        " • 과제#12 마감 D-1",
        " • QOTD 보상 +20Cr",
        " • 자리권 매진"
    };
    tui_common_print_multiline(win, 1, 2, lines, sizeof(lines) / sizeof(lines[0]));
    mvwprintw(win, getmaxy(win) - 3, 2, "QOTD: 용돈을 모으는 비법?");
    mvwprintw(win, getmaxy(win) - 2, 4, "1) 목표  2) 즉시소비  3) 랜덤투자");
    mvwprintw(win, getmaxy(win) - 1, 4, "[q] 제출 화면에서 응답");
    wrefresh(win);
}

static void draw_dashboard(User *user, const char *status) {
    erase();
    mvprintw(1, (COLS - 30) / 2, "Class Royale - 학생 대시보드");
    mvprintw(3, 2, "이름: %s | 잔액: %d Cr | 신용도: %c", user->name, user->bank.balance, user->bank.rating);
    mvprintw(4, 2, "보유 아이템: %d건 | 보유 주식: %d건", 10, user->holding_count);
    int percent = user->total_missions > 0 ? (user->completed_missions * 100) / user->total_missions : 0;
    mvprintw(5, 2, "미션 달성률:");
    tui_common_draw_progress(stdscr, 5, 18, COLS / 2, percent);

    int col_width = COLS / 2 - 3;
    int box_height = (LINES - 10) / 2;
    WINDOW *mission_win = tui_common_create_box(box_height, col_width, 7, 2, "과제/미션");
    render_mission_preview(mission_win, user);
    tui_common_destroy_box(mission_win);

    WINDOW *account_win = tui_common_create_box(box_height, col_width, 7 + box_height, 2, "계좌 현황");
    mvwprintw(account_win, 1, 2, "예금 잔액: %d Cr", user->bank.balance);
    mvwprintw(account_win, 2, 2, "세금 예상: %d Cr", econ_tax(&user->bank));
    mvwprintw(account_win, 4, 2, "최근 거래 예시");
    mvwprintw(account_win, 5, 4, "+40Cr 미션#12 보상");
    mvwprintw(account_win, 6, 4, "-80Cr 자리선점권 구매");
    wrefresh(account_win);
    tui_common_destroy_box(account_win);

    WINDOW *shop_win = tui_common_create_box(box_height, col_width, 7, col_width + 4, "상점/장터");
    render_shop_preview(shop_win);
    tui_common_destroy_box(shop_win);

    WINDOW *news_win = tui_common_create_box(box_height, col_width, 7 + box_height, col_width + 4, "알림 & QOTD");
    render_news(news_win);
    tui_common_destroy_box(news_win);

    tui_common_draw_help("m:미션 s:상점 a:계좌 k:주식 u:경매 q:로그아웃");
    tui_ncurses_draw_status(status);
    refresh();
}

static void handle_mission_board(User *user) {
    if (!user) {
        return;
    }
    int height = LINES - 4;
    int width = COLS - 6;
    WINDOW *win = tui_common_create_box(height, width, 2, 3, "미션 보드 (Enter 완료/ a 새 미션/ q 닫기)");
    int highlight = 0;
    keypad(win, TRUE);
    int running = 1;
    while (running) {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 0, 2, " 미션 보드 (Enter 완료/ a 새 미션/ q 닫기) ");
        int available = user->mission_count;
        if (available == 0) {
            mvwprintw(win, 2, 2, "할당된 미션이 없습니다. a 키로 새 미션을 받아보세요!");
        }
        for (int i = 0; i < available && i < height - 2; ++i) {
            if (i == highlight) {
                wattron(win, A_REVERSE);
            }
            Mission *mission = &user->missions[i];
            mvwprintw(win, 1 + i, 2, "#%d %-20s [%s] +%d", mission->id, mission->name,
                      mission->completed ? "완료" : "진행", mission->reward);
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
            if (mission_complete(user->name, user->missions[highlight].id)) {
                tui_ncurses_toast("미션 완료! 보상이 지급되었습니다", 900);
            } else {
                tui_ncurses_toast("미션 완료 처리 실패", 900);
            }
        } else if (ch == 'a' || ch == 'A') {
            Mission open[8];
            int count = 0;
            if (mission_list_open(open, &count) && count > 0) {
                admin_assign_mission(user->name, &open[rand() % count]);
                tui_ncurses_toast("새 미션이 추가되었습니다", 900);
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
        tui_ncurses_toast("상점 정보가 없습니다", 800);
        return;
    }
    Shop *shop = &shops[0];
    int height = LINES - 4;
    int width = COLS - 6;
    WINDOW *win = tui_common_create_box(height, width, 2, 3, "상점 (Enter 구매 / s 판매 / q 닫기)");
    int highlight = 0;
    keypad(win, TRUE);
    while (1) {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 0, 2, " %s 상점 - 잔액 %dCr ", shop->name, user->bank.balance);
        int visible = height - 2;
        for (int i = 0; i < shop->item_count && i < visible; ++i) {
            if (i == highlight) {
                wattron(win, A_REVERSE);
            }
            mvwprintw(win, 1 + i, 2, "%s %3dCr 재고:%2d", shop->items[i].name, shop->items[i].cost, shop->items[i].stock);
            if (i == highlight) {
                wattroff(win, A_REVERSE);
            }
        }
        wrefresh(win);
        int ch = wgetch(win);
        if (shop->item_count == 0) {
            mvwprintw(win, 2, 2, "등록된 상품이 없습니다");
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
                tui_ncurses_toast("구매 완료", 800);
            } else {
                tui_ncurses_toast("구매 실패 - 잔액/재고 확인", 800);
            }
        } else if (ch == 's' || ch == 'S') {
            if (shop_sell(user->name, &shop->items[highlight], 1)) {
                tui_ncurses_toast("판매 완료", 800);
            } else {
                tui_ncurses_toast("판매 실패", 800);
            }
        } else if (ch == 'q' || ch == 27) {
            break;
        }
    }
    tui_common_destroy_box(win);
}

static void handle_account_view(User *user) {
    int height = 12;
    int width = 60;
    WINDOW *win = tui_common_create_box(height, width, (LINES - height) / 2, (COLS - width) / 2,
                                        "계좌 관리 (d 예금 / b 대출 / r 상환 / q 닫기)");
    int running = 1;
    while (running) {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 0, 2, " 계좌 관리 ");
        mvwprintw(win, 1, 2, "예금: %d Cr", user->bank.balance);
        mvwprintw(win, 2, 2, "신용도: %c", user->bank.rating);
        mvwprintw(win, 3, 2, "세금 예상: %d Cr", econ_tax(&user->bank));
        mvwprintw(win, 5, 2, "명령: d)예금  b)대출  r)상환  q)닫기");
        wrefresh(win);
        int ch = wgetch(win);
        if (ch == 'd' || ch == 'b' || ch == 'r') {
            char label[32];
            if (ch == 'd') {
                snprintf(label, sizeof(label), "예금 금액");
            } else if (ch == 'b') {
                snprintf(label, sizeof(label), "대출 금액");
            } else {
                snprintf(label, sizeof(label), "상환 금액");
            }
            int amount = 0;
            if (tui_ncurses_prompt_number(win, label, &amount) && amount > 0) {
                int ok = 0;
                if (ch == 'd') {
                    ok = econ_deposit(&user->bank, amount);
                } else if (ch == 'b') {
                    ok = econ_borrow(&user->bank, amount);
                } else {
                    ok = econ_repay(&user->bank, amount);
                }
                tui_ncurses_toast(ok ? "처리되었습니다" : "거래 실패", 800);
            }
        } else if (ch == 'q' || ch == 27) {
            running = 0;
        }
    }
    tui_common_destroy_box(win);
}

void tui_student_loop(User *user, int demo_mode) {
    if (!user) {
        return;
    }
    ensure_student_seed(user);
    const char *status = "m:미션 s:상점 a:계좌 k:주식 u:경매 q:로그아웃";
    int running = 1;
    while (running) {
        draw_dashboard(user, status);
        if (demo_mode) {
            napms(800);
            break;
        }
        int ch = getch();
        switch (ch) {
            case 'm':
            case 'M':
                handle_mission_board(user);
                status = "미션 화면에서 Enter로 완료";
                break;
            case 's':
            case 'S':
                handle_shop_view(user);
                status = "상점에서 Enter로 구매";
                break;
            case 'a':
            case 'A':
                handle_account_view(user);
                status = "계좌 조작 완료";
                break;
            case 'k':
            case 'K':
                tui_stock_show_market(user);
                status = "주식 거래를 완료했습니다";
                break;
            case 'u':
            case 'U':
                tui_auction_show_house(user);
                status = "경매장에서 활동";
                break;
            case 'q':
            case 'Q':
                running = 0;
                break;
            default:
                status = "키보드 단축키를 확인하세요";
                break;
        }
    }
}
