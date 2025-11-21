#include "../../include/ui/tui.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../include/domain/user.h"
#include "../../include/ui/tui_login.h"
#include "../../include/ui/tui_ncurses.h"
#include "../../include/ui/tui_student.h"
#include "../../include/ui/tui_teacher.h"

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
        tui_ncurses_toast("Logged out. Press any key to continue", 800);
    }

    tui_ncurses_shutdown();
}
