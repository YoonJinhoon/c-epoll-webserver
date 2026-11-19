#include <stdio.h>

/* 빌드·테스트 파이프라인이 도는지만 확인하는 자리표시자. 모듈 테스트가 생기면 지운다. */
int main(void)
{
    printf("smoke: ok\n");
    return 0;
}
