/*
 * 파일 목적: tui 관련 터미널 UI 로직 구현
 * 작성자: ChatGPT
 * 작성일: 2024-06-13
 * 수정 이력: 2024-06-13 ChatGPT - 주석 규칙 적용
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

/* 함수 목적: tui_run 함수는 tui 관련 터미널 UI 로직 구현에서 필요한 동작을 수행합니다.
 * 매개변수: 없음
 * 반환 값: 함수 수행 결과를 나타냅니다.
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
