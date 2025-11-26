/*
 * 파일 목적: app 애플리케이션 구동 로직
 * 작성자: 이현준
 */
#include "../include/app.h"
#include "../include/ui/tui.h"

static int g_bootstrapped = 0;

/* 함수 목적: ui 함수로 넘어가는 역할을 한다.
 * 매개변수: 없음
 * 반환 값: 없음
 */
void app_bootstrap(void) {
    if (g_bootstrapped) {
        return;
    }
    g_bootstrapped = 1;
    tui_run();
}

/* 함수 목적: 앱을 종료한다.
 * 매개변수: 없음
 * 반환 값: 없음
 */
void app_shutdown(void) {
    g_bootstrapped = 0;
}
