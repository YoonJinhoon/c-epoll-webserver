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
