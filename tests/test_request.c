/* 단위 테스트: request_head_end (요청 머리 끝 찾기)
 * 서버를 띄우지 않고 함수 하나만 검사한다. `make test` 로 실행된다. */
#include <stdio.h>
#include <string.h>

#include "request.h"

static int failed = 0;

static void check(const char *name, const char *buf, size_t expected)
{
    size_t got = request_head_end(buf, strlen(buf));
    if (got == expected) {
        printf("  PASS  %s\n", name);
    } else {
        printf("  FAIL  %s (기대 %zu, 실제 %zu)\n", name, expected, got);
        failed = 1;
    }
}

int main(void)
{
    printf("request_head_end\n");
    check("빈 입력",                     "",                                 0);
    check("요청 라인 중간까지만",          "GET / HT",                         0);
    check("줄바꿈 하나뿐 (\\r\\n)",         "GET / HTTP/1.1\r\n",               0);
    check("\\n 만으로 빈 줄 (\\r 없음)",    "GET / HTTP/1.1\n\n",               0);
    check("완전한 머리",                  "GET / HTTP/1.1\r\n\r\n",           18);
    check("헤더 있는 완전한 머리",         "GET / HTTP/1.1\r\nHost: a\r\n\r\n", 27);
    check("머리 뒤에 본문이 붙어 있음",     "POST / HTTP/1.1\r\n\r\nhello",     19);
    return failed;
}
