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

#endif
