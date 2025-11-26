/*
 * 파일 목적: app 애플리케이션 구동 로직
 * 작성자: ChatGPT
 * 작성일: 2024-06-13
 * 수정 이력: 2024-06-13 ChatGPT - 주석 규칙 적용
 */
#include "../include/app.h"

#include "../include/ui/tui.h"

static int g_bootstrapped = 0;

/* 함수 목적: app_bootstrap 함수는 app 애플리케이션 구동 로직에서 필요한 동작을 수행합니다.
 * 매개변수: 없음
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
void app_bootstrap(void) {
    if (g_bootstrapped) {
        return;
    }
    g_bootstrapped = 1;
    tui_run();
}

/* 함수 목적: app_shutdown 함수는 app 애플리케이션 구동 로직에서 필요한 동작을 수행합니다.
 * 매개변수: 없음
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
void app_shutdown(void) {
    g_bootstrapped = 0;
}
