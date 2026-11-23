/* 웹 서버 (S1: epoll 뼈대 + 요청을 끝까지 읽기)
 *
 * 사용법: ./webserver <port> <static_root>      (static_root 는 S3 에서 쓴다)
 *
 * 구조: 스레드는 main 하나. epoll 로 여러 연결을 지켜본다 (실험 04 와 같음).
 * S1 에서 새로 생긴 것: 연결마다 "보관함"(struct conn)을 하나씩 두고,
 *   조각이 도착할 때마다 보관함 뒤에 이어 붙인다.
 *   머리 끝(\r\n\r\n)이 보이면 그때 응답한다. 안 보이면 epoll_wait 로 돌아가 다른 연결을 처리한다.
 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

#include "request.h"

#define MAX_FDS     1024    /* 이 번호 이상의 fd 는 받지 않는다 (보관함 표 크기) */
#define BUF_SIZE    8192    /* 보관함 하나의 크기 = 요청 머리 최대 길이 */
#define MAX_EVENTS  64

/* 연결 하나의 보관함 */
struct conn {
    int    fd;              /* 어느 연결의 보관함인가 */
    char   buf[BUF_SIZE];   /* 지금까지 도착한 조각들을 이어 붙인 것 */
    size_t len;             /* 지금까지 모인 바이트 수 = 다음 조각을 붙일 위치 */
};

static struct conn *conns[MAX_FDS];   /* fd 번호로 보관함을 찾는 표 */

/* ---------- 연결이 생길 때 / 끝날 때 ---------- */

static void conn_open(int epfd, int fd)
{
    if (fd >= MAX_FDS) {                      /* 표에 자리가 없음 */
        close(fd);
        return;
    }
    struct conn *c = calloc(1, sizeof *c);   /* 보관함 하나 만들기 (len = 0) */
    if (c == NULL) {
        close(fd);
        return;
    }
    c->fd = fd;
    conns[fd] = c;

    struct epoll_event ev = { .events = EPOLLIN, .data.fd = fd };
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);  /* "이 연결에 데이터 오면 깨워줘" */
    printf("[fd %d] 연결됨 -> 보관함 만들고 등록\n", fd);
}

static void conn_close(int epfd, struct conn *c)
{
    epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, NULL);
    close(c->fd);
    conns[c->fd] = NULL;
    printf("[fd %d] close, 보관함 버림\n", c->fd);
    free(c);
}

/* ---------- 응답 (S1 에서는 항상 같은 응답) ---------- */

static void send_response(struct conn *c)
{
    const char *body = "hello from webserver (S1)\n";
    char resp[256];
    int n = snprintf(resp, sizeof resp,
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Type: text/plain\r\n"
                     "Content-Length: %zu\r\n"
                     "Connection: close\r\n"
                     "\r\n"
                     "%s",
                     strlen(body), body);
    write(c->fd, resp, (size_t)n);
}

/* ---------- 조각 하나가 도착했을 때 ---------- */

static void conn_on_data(int epfd, struct conn *c)
{
    size_t room = BUF_SIZE - c->len;          /* 보관함에 남은 자리 */
    if (room == 0) {                          /* 꽉 찼는데도 머리 끝이 없음 */
        printf("[fd %d] 요청 머리가 %d 바이트를 넘음 -> 끊음\n", c->fd, BUF_SIZE);
        conn_close(epfd, c);
        return;
    }

    /* 새 조각을 보관함의 len 위치부터 이어 붙인다 */
    ssize_t n = read(c->fd, c->buf + c->len, room);
    if (n <= 0) {                             /* 0: 상대가 끊음, -1: 에러 */
        conn_close(epfd, c);
        return;
    }
    c->len += (size_t)n;
    printf("[fd %d] 조각 %zd 바이트 도착 -> 보관함 %zu 바이트\n", c->fd, n, c->len);

    size_t head = request_head_end(c->buf, c->len);
    if (head == 0) {
        printf("[fd %d] 머리 끝(\\r\\n\\r\\n) 아직 없음 -> 기다림 (다른 연결 처리하러 감)\n", c->fd);
        return;                               /* epoll_wait 로 돌아간다 */
    }

    printf("[fd %d] 머리 끝 발견 (머리 %zu 바이트) -> 응답 1번\n", c->fd, head);
    send_response(c);
    conn_close(epfd, c);
}

/* ---------- 시작 ---------- */

static int open_listen(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);
    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("bind");
        exit(1);
    }
    listen(fd, 128);
    return fd;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s <port> <static_root>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);

    setvbuf(stdout, NULL, _IOLBF, 0);         /* 로그를 줄마다 바로 출력 */
    signal(SIGPIPE, SIG_IGN);                 /* 응답 도중 클라이언트가 끊어도 서버가 죽지 않게 (S6) */

    int listen_fd = open_listen(port);
    int epfd = epoll_create1(0);
    struct epoll_event ev = { .events = EPOLLIN, .data.fd = listen_fd };
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);
    printf("포트 %d 준비 완료 (root=%s)\n", port, argv[2]);

    struct epoll_event events[MAX_EVENTS];
    for (;;) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;
            if (fd == listen_fd) {
                int conn_fd = accept(listen_fd, NULL, NULL);
                if (conn_fd >= 0)
                    conn_open(epfd, conn_fd);
            } else if (fd < MAX_FDS && conns[fd] != NULL) {
                conn_on_data(epfd, conns[fd]);
            }
        }
    }
}
