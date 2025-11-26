/*
 *파일 이름: main.c
 *파일 목적: main 함수 구동
 * 작성자: 이현준
 */
#include "../include/app.h"

/* 함수 목적: 프로그램을 시작
 * 매개변수: 없음
 * 반환 값: 함수 수행 결과를 나타냅니다.
 */
int main(void) {
    app_bootstrap();
    app_shutdown();
    return 0;
}
    