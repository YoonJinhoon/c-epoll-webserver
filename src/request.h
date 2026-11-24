#ifndef REQUEST_H
#define REQUEST_H

#include <stddef.h>

/* 요청 머리(요청 라인 + 헤더)가 끝까지 왔는지 확인한다.
 *
 * 입력: buf[0 .. len) — 지금까지 한 연결에서 모은 바이트
 * 출력: 머리 끝 표시(빈 줄, "\r\n\r\n")가 있으면 → 그 끝까지의 길이 (빈 줄 포함)
 *       아직 없으면                              → 0 ("더 기다려야 함")
 */
size_t request_head_end(const char *buf, size_t len);

/* 요청 라인(첫 줄)을 쪼갠 결과 */
struct request_line {
    char method[16];     /* 예: "GET"      */
    char path[2048];     /* 예: "/hello"   */
    char version[16];    /* 예: "HTTP/1.1" */
};

/* 요청 라인을 세 조각으로 쪼갠다.
 *
 * 입력: buf[0 .. len) — 요청 머리 (첫 줄이 "\r\n" 으로 끝나 있어야 함)
 * 출력: 성공 → 0, out 에 세 조각을 채움
 *       규칙에 안 맞음 → -1 (서버는 400 으로 답한다)
 *
 * 규칙: "메서드 공백 경로 공백 버전" 정확히 3조각 (공백은 한 칸씩)
 *       메서드는 대문자, 경로는 '/' 로 시작, 버전은 "HTTP/" 로 시작
 */
int request_parse_line(const char *buf, size_t len, struct request_line *out);

/* 경로가 루트 폴더 밖으로 나갈 수 있는가? (S3-c, 경로 탈출 막기)
 *
 * 입력: 요청 경로 (예: "/index.html", "/../src/main.c")
 * 출력: 안전 → 1
 *       경로 조각 중에 ".." 가 하나라도 있으면 → 0 (서버는 403 으로 답한다)
 *
 * 예) "/a/../b" 도 0. 실제로는 루트 안에 머물지만, 판단을 단순하게 하려고 ".." 자체를 금지한다 */
int path_is_safe(const char *path);

#endif
