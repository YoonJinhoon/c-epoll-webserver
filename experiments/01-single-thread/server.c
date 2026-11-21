/* 실험 01: 단일 스레드 서버 (버리는 코드)
 *
 * 목적: "서버가 한 번에 한 연결만 처리하면 무슨 일이 생기나"를 직접 본다.
 * 일부러 단순하게 만들었다:
 *   - 스레드 1개 (main 함수를 실행하는 흐름 하나뿐)
 *   - read() 를 딱 한 번만 부른다 (요청이 조각나서 오는 문제는 무시)
 *   - 요청 내용과 상관없이 항상 같은 응답을 보낸다
 *
 * 빌드:  gcc -Wall -Wextra -o server server.c
 * 실행:  ./server            (포트 9000)
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>      /* read, write, close */
#include <arpa/inet.h>   /* socket, bind, listen, accept, htons */

#define PORT 9000

int main(void)
{
    /* ① listen 소켓 만들기: 새 연결을 "받기만" 하는 소켓. 반환값은 fd(번호) */
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);   /* SOCK_STREAM = TCP */
    printf("[1] socket()  -> listen 소켓 fd = %d\n", listen_fd);

    /* 지금은 몰라도 됨. 이 줄을 지우고 서버를 껐다 바로 켜 보면 무슨 일이? (questions.md) */
    int yes = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    /* ② bind: 이 소켓을 "내 컴퓨터의 9000번 포트"에 묶는다 */
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);   /* 모든 네트워크 주소에서 받음 */
    addr.sin_port = htons(PORT);
    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("bind");
        return 1;
    }

    /* ③ listen: 이제부터 연결을 받겠다고 커널에 알린다 */
    listen(listen_fd, 16);
    printf("[2] bind + listen 완료. http://localhost:%d 기다리는 중\n", PORT);

    for (;;) {
        /* ④ accept: 새 연결이 올 때까지 이 스레드가 잠든다.
         *    연결이 오면 그 클라이언트 전용 "연결 소켓" fd 를 새로 돌려준다 */
        printf("\n[3] accept() 호출 -> 새 연결 올 때까지 잠듦...\n");
        fflush(stdout);
        int conn_fd = accept(listen_fd, NULL, NULL);
        printf("[4] accept() 깨어남 -> 연결 소켓 fd = %d\n", conn_fd);

        /* ⑤ read: 요청 데이터가 올 때까지 이 스레드가 잠든다 */
        char buf[4096];
        printf("[5] read() 호출 -> 요청 데이터 올 때까지 잠듦...\n");
        fflush(stdout);
        ssize_t n = read(conn_fd, buf, sizeof buf - 1);
        if (n <= 0) {                       /* 0: 상대가 끊음, -1: 에러 */
            printf("[6] read() = %zd -> 요청 없이 끝남, 연결 닫음\n", n);
            close(conn_fd);
            continue;
        }
        buf[n] = '\0';
        printf("[6] read() 깨어남 -> %zd 바이트 받음:\n----\n%s----\n", n, buf);

        /* ⑥ write: 응답 보내기 (상태 줄, 헤더, 빈 줄, 본문) */
        const char *body = "hello from single-thread server\n";
        char resp[512];
        int len = snprintf(resp, sizeof resp,
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/plain\r\n"
                           "Content-Length: %zu\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "%s",
                           strlen(body), body);
        write(conn_fd, resp, len);
        printf("[7] 응답 %d 바이트 보냄, 연결 소켓 fd = %d 닫음\n", len, conn_fd);

        /* ⑦ 이 연결은 끝. 다시 ④ accept 로 돌아간다 */
        close(conn_fd);
    }
}
