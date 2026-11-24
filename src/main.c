/* 웹 서버 (S1: epoll 뼈대 + 요청을 끝까지 읽기 / S2: 요청 라인 파싱 / S3: 정적 파일)
 *
 * 사용법: ./webserver <port> <static_root>      예) ./webserver 9000 www
 *
 * 구조: 스레드는 main 하나. epoll 로 여러 연결을 지켜본다 (실험 04 와 같음).
 * S1 에서 새로 생긴 것: 연결마다 "보관함"(struct conn)을 하나씩 두고,
 *   조각이 도착할 때마다 보관함 뒤에 이어 붙인다.
 *   머리 끝(\r\n\r\n)이 보이면 그때 응답한다. 안 보이면 epoll_wait 로 돌아가 다른 연결을 처리한다.
 */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/stat.h>

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
static const char  *g_root;           /* 정적 파일 루트 폴더 (예: "www") */

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

/* ---------- 응답 ---------- */

/* len 바이트를 "전부" 보낸다. write 는 덜 보낼 수 있으므로, 다 보낼 때까지 반복한다.
 * 성공 0, 실패 -1 */
static int write_all(int fd, const char *p, size_t len)
{
    while (len > 0) {
        ssize_t n = write(fd, p, len);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        p   += n;          /* 보낸 만큼 앞으로 */
        len -= (size_t)n;  /* 남은 양 줄이기 */
    }
    return 0;
}

/* 상태 코드(200, 400 ...)와 본문을 받아서 응답 하나를 보낸다 */
static void send_response(struct conn *c, int status, const char *reason, const char *body)
{
    char resp[4096];
    int n = snprintf(resp, sizeof resp,
                     "HTTP/1.1 %d %s\r\n"
                     "Content-Type: text/plain\r\n"
                     "Content-Length: %zu\r\n"
                     "Connection: close\r\n"
                     "\r\n"
                     "%s",
                     status, reason, strlen(body), body);
    if (n < 0 || (size_t)n >= sizeof resp)
        return;
    write_all(c->fd, resp, (size_t)n);
}

/* S3: 경로에 해당하는 파일을 루트 폴더에서 찾아 보낸다.
 *   /index.html → www/index.html
 *   /           → www/index.html  (경로가 / 로 끝나면 index.html 을 붙인다)
 * 경로에 ".." 가 있으면 403 (루트 밖으로 나가는 것 금지, S3-c).
 * 파일이 없으면 404, 읽을 권한이 없으면 403, 그 밖의 문제는 500. */
static void serve_file(struct conn *c, const char *path)
{
    /* 0. S3-c: 루트 밖으로 나가는 경로("..")는 파일을 찾기 전에 막는다 */
    if (!path_is_safe(path)) {
        printf("[fd %d] 경로 %s 에 \"..\" 있음 (루트 밖으로 나가려 함) -> 403\n", c->fd, path);
        send_response(c, 403, "Forbidden", "forbidden\n");
        return;
    }

    /* 1. 실제 파일 위치 = 루트 + 경로 */
    char file[4200];
    size_t plen = strlen(path);
    snprintf(file, sizeof file, "%s%s%s", g_root, path,
             (plen > 0 && path[plen - 1] == '/') ? "index.html" : "");

    /* 2. 파일 열기 */
    int ffd = open(file, O_RDONLY);
    if (ffd < 0) {
        if (errno == ENOENT || errno == ENOTDIR) {
            printf("[fd %d] 파일 %s 없음 -> 404\n", c->fd, file);
            send_response(c, 404, "Not Found", "not found\n");
        } else if (errno == EACCES) {
            printf("[fd %d] 파일 %s 읽을 권한 없음 -> 403\n", c->fd, file);
            send_response(c, 403, "Forbidden", "forbidden\n");
        } else {
            printf("[fd %d] 파일 %s 열기 실패 -> 500\n", c->fd, file);
            send_response(c, 500, "Internal Server Error", "internal error\n");
        }
        return;
    }

    /* 3. 일반 파일인지, 크기는 몇인지 */
    struct stat st;
    if (fstat(ffd, &st) < 0 || !S_ISREG(st.st_mode)) {   /* 폴더 등은 보내지 않는다 */
        printf("[fd %d] %s 는 일반 파일이 아님 -> 404\n", c->fd, file);
        close(ffd);
        send_response(c, 404, "Not Found", "not found\n");
        return;
    }

    /* 4. 머리 보내기 (본문 길이 = 파일 크기) */
    char head[256];
    int hn = snprintf(head, sizeof head,
                      "HTTP/1.1 200 OK\r\n"
                      "Content-Type: %s\r\n"
                      "Content-Length: %lld\r\n"
                      "Connection: close\r\n"
                      "\r\n",
                      mime_type(file),                  /* S3-b: 확장자별 MIME 타입 */
                      (long long)st.st_size);
    write_all(c->fd, head, (size_t)hn);

    /* 5. 본문 = 파일 내용을 조금씩 읽어서 그대로 보내기 */
    char chunk[8192];
    ssize_t n;
    while ((n = read(ffd, chunk, sizeof chunk)) > 0) {
        if (write_all(c->fd, chunk, (size_t)n) < 0)
            break;
    }
    close(ffd);
    printf("[fd %d] 파일 %s (%lld 바이트) 보냄 -> 200\n", c->fd, file, (long long)st.st_size);
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

    printf("[fd %d] 머리 끝 발견 (머리 %zu 바이트)\n", c->fd, head);

    /* S2: 요청 라인을 세 조각으로 쪼갠다 */
    struct request_line rl;
    if (request_parse_line(c->buf, head, &rl) < 0) {
        printf("[fd %d] 요청 라인이 규칙에 안 맞음 -> 400\n", c->fd);
        send_response(c, 400, "Bad Request", "bad request line\n");
    } else if ( strcmp(rl.method, "GET") != 0 && strcmp(rl.method, "POST") != 0 ) {
        printf("[fd %d] method=%s 지원 안 함 -> 501\n", c->fd, rl.method);
        send_response(c, 501, "Not Implemented", "method not supported\n");
    } else if (strcmp(rl.method, "GET") == 0) {
        /* S3: GET 은 파일을 찾아서 보낸다 */
        printf("[fd %d] 파싱 성공: method=%s path=%s\n", c->fd, rl.method, rl.path);
        serve_file(c, rl.path);
    } else {
        /* POST: 본문 처리는 S4. 지금은 S2 처럼 파싱 결과만 돌려준다 */
        printf("[fd %d] 파싱 성공: method=%s path=%s version=%s -> 200\n",
               c->fd, rl.method, rl.path, rl.version);
        char body[2200];
        snprintf(body, sizeof body, "method=%s\npath=%s\nversion=%s\n",
                 rl.method, rl.path, rl.version);
        send_response(c, 200, "OK", body);
    }
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
    g_root = argv[2];

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
