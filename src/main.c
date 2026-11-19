#include <stdio.h>
#include <stdlib.h>

/* 사용법: ./webserver <port> <static_root>
 * 서버 구현 전 자리표시자. 인자만 확인하고 종료한다. */
int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s <port> <static_root>\n", argv[0]);
        return 1;
    }
    printf("port=%s root=%s (not implemented yet)\n", argv[1], argv[2]);
    return 0;
}
