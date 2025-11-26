#include "../include/app.h"

int main(void) {
    load_seats_csv();
    app_bootstrap();
    app_shutdown();
    return 0;
}
    