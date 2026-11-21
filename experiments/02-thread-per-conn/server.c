/* 실험 02: 연결마다 스레드 1개 (버리는 코드)
 *
 * 실험 01과 달라진 점은 딱 하나:
 *   main 스레드는 accept 만 하고, 연결이 오면 "그 연결 담당 스레드"를 새로 만든다.
 *   read / write / close 는 담당 스레드가 한다.
 * 로그 앞의 [main], [T1], [T2]... 가 "어느 스레드가 찍었나"를 알려준다.
 *
 * 빌드:  gcc -Wall -Wextra -pthread -o server server.c
 * 실행:  ./server            (포트 9000)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>     /* pthread_create, pthread_detach */
#include <arpa/inet.h>

#define PORT 9000

struct job {
    int conn_fd;   /* 이 스레드가 맡을 연결 소켓 */
    int id;        /* 로그용 번호: T1, T2, ... */
};

/* 담당 스레드가 실행하는 함수: 클라이언트 하나 처리하고 끝 */
static void *handle_client(void *arg)
{
    struct job *job = arg;
    int fd = job->conn_fd, id = job->id;
    free(job);

    char buf[4096];
    printf("[T%d] [5] read() 호출 -> 요청 데이터 올 때까지 잠듦...\n", id);
    fflush(stdout);
    ssize_t n = read(fd, buf, sizeof buf - 1);
    if (n <= 0) {
        printf("[T%d] [6] read() = %zd -> 요청 없이 끝남\n", id, n);
        close(fd);
        return NULL;
    }
    buf[n] = '\0';
    printf("[T%d] [6] read() 깨어남 -> %zd 바이트 받음\n", id, n);

    const char *body = "hello from thread-per-connection server\n";
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
    printf("[T%d] [7] 응답 보냄, fd = %d 닫음 -> 이 스레드 끝\n", id, fd);
    return NULL;   /* 함수가 끝나면 스레드도 끝난다 */
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
    printf("[main] [1][2] listen 소켓 fd = %d, 포트 %d 준비 완료\n", listen_fd, PORT);

    int next_id = 1;
    for (;;) {
        printf("\n[main] [3] accept() 호출 -> 새 연결 올 때까지 잠듦...\n");
        fflush(stdout);
        int conn_fd = accept(listen_fd, NULL, NULL);
        printf("[main] [4] accept() 깨어남 -> 연결 소켓 fd = %d\n", conn_fd);

        /* 실험 01과 다른 부분: read 를 직접 하지 않고 담당 스레드에게 넘긴다 */
        struct job *job = malloc(sizeof *job);
        job->conn_fd = conn_fd;
        int id = next_id++;
        job->id = id;
        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, job);   /* 이후 job 은 담당 스레드 것 */
        pthread_detach(tid);   /* 끝나면 알아서 정리되게 (몰라도 됨) */
        printf("[main]     T%d 만들고 fd = %d 넘김 -> 다시 accept 로\n", id, conn_fd);
    }
}
