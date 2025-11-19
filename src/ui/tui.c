#include "../../include/ui/tui.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../include/domain/user.h"
#include "../../include/ui/tui_login.h"
#include "../../include/ui/tui_ncurses.h"
#include "../../include/ui/tui_student.h"
#include "../../include/ui/tui_teacher.h"

void tui_run(void) {
    tui_ncurses_init();
    srand((unsigned)time(NULL));
    const char *auto_mode = getenv("CLASSROYALE_AUTODEMO");
    int demo_mode = auto_mode && *auto_mode;
    User *auto_user = NULL;
    if (demo_mode) {
        if (auto_mode && strncmp(auto_mode, "teacher", 7) == 0) {
            auto_user = user_lookup("teacher");
        } else {
            auto_user = user_lookup("student");
        }
    }

    while (1) {
        User *user = auto_user;
        if (!user) {
            user = tui_login_flow(demo_mode);
        }
        if (!user) {
            break;
        }
        if (user->isadmin == TEACHER) {
            tui_teacher_loop(user, demo_mode && user == auto_user);
        } else {
            tui_student_loop(user, demo_mode && user == auto_user);
        }
        if (demo_mode) {
            break;
        }
        tui_ncurses_toast("로그아웃되었습니다. 아무 키나 눌러 계속", 800);
    }

    tui_ncurses_shutdown();
}
