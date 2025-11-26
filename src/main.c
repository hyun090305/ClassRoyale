/*
 * 파일 목적: main 애플리케이션 구동 로직
 * 작성자: ChatGPT
 * 작성일: 2024-06-13
 * 수정 이력: 2024-06-13 ChatGPT - 주석 규칙 적용
 */
#include "../include/app.h"

/* 함수 목적: main 함수는 main 애플리케이션 구동 로직에서 필요한 동작을 수행합니다.
 * 매개변수: 없음
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
int main(void) {
    app_bootstrap();
    app_shutdown();
    return 0;
}
    