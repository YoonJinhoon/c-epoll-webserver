/* 실험 04: epoll (버리는 코드)
 *
 * 스레드는 main 하나뿐이다 (실험 01과 같음). 그런데 이번엔 read 에서 묶이지 않는다.
 *
 * 방법:
 *   1. 지켜볼 fd 들을 커널에 "등록"해 둔다 (처음엔 listen 소켓 하나)
 *   2. epoll_wait 에서 잠든다 -> 등록된 fd 중 하나라도 "할 일이 생기면" 커널이 깨워준다
 *   3. 깨어나면 커널이 알려준 fd 만 처리한다
 *        - listen 소켓에 할 일 = 새 연결이 옴  -> accept 하고, 새 fd 도 등록
 *        - 연결 fd 에 할 일   = 요청 데이터가 옴 -> read (데이터가 이미 와 있으니 안 잠든다) -> write -> close
 *   4. 다시 2로
 *
 * 실험용 단순화 (몰라도 됨): non-blocking 설정과 "요청이 조각나서 오는 경우"는 생략했다.
 *
 * 빌드:  gcc -Wall -Wextra -o server server.c
 * 실행:  ./server            (포트 9000)
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>     /* epoll_create1, epoll_ctl, epoll_wait */

#define PORT 9000
#define MAX_EVENTS 16

static int watching = 0;   /* 지금 등록된 연결 fd 개수 (로그용) */

static void handle_request(int epfd, int fd)
{
    char buf[4096];
    ssize_t n = read(fd, buf, sizeof buf - 1);   /* 데이터가 이미 와 있으므로 바로 끝난다 */

    /* 처리가 끝나면 이 fd 는 더 지켜볼 필요가 없다 -> 등록 해제 */
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
    watching--;

    if (n <= 0) {
        printf("[main]   read(%d) = %zd -> 상대가 끊음, close (지켜보는 연결 %d개)\n", fd, n, watching);
        close(fd);
        return;
    }
    printf("[main]   read(%d) -> %zd 바이트, 안 잠들고 바로 읽음\n", fd, n);

    const char *body = "hello from epoll server\n";
    char resp[512];
    int len = snprintf(resp, sizeof resp,
                       "HTTP/1.1 200 OK\r\n"
                       "Content-Type: text/plain\r\n"
                       "Content-Length: %zu\r\n"
                       "Connection: close\r\n"
                       "\r\n"
                       "%s",
                       strlen(body), body);
    write(fd, resp, len);
    close(fd);
    printf("[main]   fd %d 응답 보내고 close (지켜보는 연결 %d개)\n", fd, watching);
}

int main(void)
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);
    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("bind");
        return 1;
    }
    listen(listen_fd, 16);

    /* ① epoll 만들기: 커널 안에 "지켜볼 fd 목록"이 생긴다 */
    int epfd = epoll_create1(0);

    /* ② listen 소켓을 목록에 등록: "새 연결이 오면 깨워줘" */
    struct epoll_event ev = {0};
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);
    printf("[main] 포트 %d 준비 완료. 스레드는 main 하나뿐. listen fd %d 등록\n", PORT, listen_fd);

    struct epoll_event events[MAX_EVENTS];
    for (;;) {
        /* ③ 등록된 fd 중 할 일이 생길 때까지 잠든다 */
        printf("\n[main] epoll_wait -> 등록된 fd 중 할 일 생길 때까지 잠듦💤 (지켜보는 연결 %d개)\n", watching);
        fflush(stdout);
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);

        /* ④ 커널이 알려준 fd 만 처리한다 */
        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            if (fd == listen_fd) {
                int conn_fd = accept(listen_fd, NULL, NULL);
                ev.events = EPOLLIN;
                ev.data.fd = conn_fd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, conn_fd, &ev);   /* "이 연결에 데이터 오면 깨워줘" */
                watching++;
                printf("[main] ⏰ listen fd에 새 연결 -> accept -> fd %d 생김, 등록 (지켜보는 연결 %d개)\n",
                       conn_fd, watching);
            } else {
                printf("[main] ⏰ fd %d에 요청 데이터 도착\n", fd);
                handle_request(epfd, fd);
            }
        }
    }
}
