#include <string.h>

#include "request.h"

size_t request_head_end(const char *buf, size_t len)
{
    /* buf 를 앞에서부터 한 칸씩 밀면서 "\r\n\r\n" 4바이트와 같은 곳을 찾는다 */
    for (size_t i = 0; i + 4 <= len; i++) {
        if (memcmp(buf + i, "\r\n\r\n", 4) == 0)
            return i + 4;     /* 빈 줄까지 포함한 길이 */
    }
    return 0;                 /* 아직 머리가 다 안 옴 */
}

/* buf[start .. end) 를 dst 에 복사한다. 비어 있거나 dst 보다 길면 -1 */
static int copy_piece(char *dst, size_t dst_size, const char *start, const char *end)
{
    size_t n = (size_t)(end - start);
    if (n == 0 || n >= dst_size)
        return -1;
    memcpy(dst, start, n);
    dst[n] = '\0';
    return 0;
}

int path_is_safe(const char *path)
{
    /* 경로를 '/' 로 나눈 조각들을 하나씩 보면서, 조각이 정확히 ".." 인지 검사한다
     *   "/../src/main.c" → 조각: "", "..", "src", "main.c" → ".." 있음 → 0 */
    const char *p = path;
    while (*p) {
        const char *start = p;
        while (*p && *p != '/')        /* 다음 '/' 까지가 조각 하나 */
            p++;
        if (p - start == 2 && start[0] == '.' && start[1] == '.')
            return 0;
        if (*p == '/')
            p++;                       /* '/' 건너뛰고 다음 조각으로 */
    }
    return 1;
}

const char *mime_type(const char *filename)
{
    const char *dot = strrchr(filename, '.');   /* 마지막 '.' 의 위치. 없으면 NULL */
    if (dot == NULL)
        return "application/octet-stream";

    /* ===== 여기부터 진훈 작성 =====
     * dot 은 ".html" 같은 확장자 글자를 가리킨다.
     * strcmp 로 비교해서 맞는 MIME 타입 글자를 return 한다. */

    if (strcmp(dot, ".html") == 0)
        return "text/html";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".js") == 0)
        return "text/javascript";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";

    /* ===== 여기까지 ===== */

    return "application/octet-stream";          /* 모르는 확장자 */
}

int request_parse_line(const char *buf, size_t len, struct request_line *out)
{
    /* 1. 첫 줄의 끝("\r\n")을 찾는다 */
    const char *line_end = NULL;
    for (size_t i = 0; i + 2 <= len; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n') {
            line_end = buf + i;
            break;
        }
    }
    if (line_end == NULL)
        return -1;

    /* 2. 첫 줄 안에서 공백 두 개를 찾는다. 정확히 두 개여야 3조각이다 */
    const char *sp1 = NULL, *sp2 = NULL;
    for (const char *p = buf; p < line_end; p++) {
        if (*p != ' ')
            continue;
        if (sp1 == NULL)       sp1 = p;
        else if (sp2 == NULL)  sp2 = p;
        else                   return -1;     /* 공백이 3개 이상 → 4조각 이상 */
    }
    if (sp2 == NULL)
        return -1;                            /* 공백이 2개 미만 → 3조각 미만 */

    /* 3. 세 조각을 잘라서 담는다 (빈 조각이나 너무 긴 조각은 실패) */
    if (copy_piece(out->method,  sizeof out->method,  buf,     sp1)      < 0 ||
        copy_piece(out->path,    sizeof out->path,    sp1 + 1, sp2)      < 0 ||
        copy_piece(out->version, sizeof out->version, sp2 + 1, line_end) < 0)
        return -1;

    /* 4. 조각마다 모양 검사 */
    for (const char *m = out->method; *m; m++)
        if (*m < 'A' || *m > 'Z')
            return -1;                        /* 메서드는 대문자만 */
    if (out->path[0] != '/')
        return -1;                            /* 경로는 '/' 로 시작 */
    if (strncmp(out->version, "HTTP/", 5) != 0)
        return -1;                            /* 버전은 "HTTP/" 로 시작 */

    return 0;
}
