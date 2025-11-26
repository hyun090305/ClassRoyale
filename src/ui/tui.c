/*
 * 파일 목적: tui 관련 터미널 UI 로직 구현
 * 작성자: 이현준
 */
#include "../../include/ui/tui.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../include/domain/user.h"
#include "../../include/ui/tui_login.h"
#include "../../include/ui/tui_ncurses.h"
#include "../../include/ui/tui_student.h"
#include "../../include/ui/tui_teacher.h"

/* 함수 목적: 시작하였을 때 어떤 상황으로 전환할지 결정
 * 매개변수: 없음
 * 반환 값: 없음
 */
void tui_run() {
    tui_ncurses_init();
    srand((unsigned)time(NULL));

    while (1) {
        User *user = tui_login_flow();
        if (!user) {
            break;
        }
        if (user->isadmin == TEACHER) {
            tui_teacher_loop(user);
        } else {
            tui_student_loop(user);
        }
        tui_ncurses_toast("Logging out...", 800);
        clear();
        refresh();
    }

    tui_ncurses_shutdown();
}
