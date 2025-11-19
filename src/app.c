#include "app.h"

static int g_bootstrapped = 0;

void app_bootstrap(void) {
    g_bootstrapped = 1;
}

void app_shutdown(void) {
    g_bootstrapped = 0;
}
