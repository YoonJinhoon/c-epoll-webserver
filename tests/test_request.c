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

/* 요청 라인 파싱: 성공해야 하는 경우 */
static void check_line_ok(const char *name, const char *buf,
                          const char *method, const char *path, const char *version)
{
    struct request_line rl;
    int r = request_parse_line(buf, strlen(buf), &rl);
    if (r == 0 && strcmp(rl.method, method) == 0 && strcmp(rl.path, path) == 0 &&
        strcmp(rl.version, version) == 0) {
        printf("  PASS  %s\n", name);
    } else {
        printf("  FAIL  %s (기대: 성공 %s|%s|%s)\n", name, method, path, version);
        failed = 1;
    }
}

/* 요청 라인 파싱: 실패(-1)해야 하는 경우 */
static void check_line_bad(const char *name, const char *buf)
{
    struct request_line rl;
    if (request_parse_line(buf, strlen(buf), &rl) == -1) {
        printf("  PASS  %s\n", name);
    } else {
        printf("  FAIL  %s (기대: -1, 실제: 성공 %s|%s|%s)\n", name, rl.method, rl.path, rl.version);
        failed = 1;
    }
}

static void test_parse_line(void)
{
    printf("request_parse_line\n");
    check_line_ok("정상 GET",               "GET /hello HTTP/1.1\r\n\r\n", "GET", "/hello", "HTTP/1.1");
    check_line_ok("정상 POST, 헤더 있음",     "POST /a/b HTTP/1.0\r\nHost: x\r\n\r\n", "POST", "/a/b", "HTTP/1.0");
    check_line_bad("6조각 (요청 두 개를 이어 씀)", "POST /hello HTTP/1.1 GET /hello HTTP/1.1\r\n\r\n");
    check_line_bad("2조각 (메서드 빠짐)",      "/hello HTTP/1.1\r\n\r\n");
    check_line_bad("공백 두 칸",              "GET  /hello HTTP/1.1\r\n\r\n");
    check_line_bad("메서드 소문자",            "get /hello HTTP/1.1\r\n\r\n");
    check_line_bad("경로가 /로 시작 안 함",     "GET hello HTTP/1.1\r\n\r\n");
    check_line_bad("버전이 HTTP/ 아님",        "GET /hello FTP/1.1\r\n\r\n");
    check_line_bad("줄 끝(\\r\\n) 없음",        "GET /hello HTTP/1.1");
}

int main(void)
{
    test_parse_line();
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
