#include "app.h"

#include "ui/tui.h"

static int g_bootstrapped = 0;

void app_bootstrap(void) {
    if (g_bootstrapped) {
        return;
    }
    g_bootstrapped = 1;
    tui_run();
}

void app_shutdown(void) {
    g_bootstrapped = 0;
}
