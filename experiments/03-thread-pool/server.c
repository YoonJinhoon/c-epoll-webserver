/* 실험 03: 스레드 풀 (버리는 코드)
 *
 * 실험 02와 달라진 점:
 *   - Worker 스레드를 처음에 WORKERS 개만 만들어 두고, 더는 만들지 않는다.
 *   - main 은 accept 로 받은 fd 를 "② 일감 대기열"(우리 프로그램 메모리 안의 배열)에 넣는다.
 *   - Worker 는 ②에서 fd 를 꺼내 read → write → close 하고, 다시 ②로 돌아간다.
 *
 * 로그에서 볼 것: 💤 = 잠듦, ⏰ = 깨어남
 *   [main] accept💤 / ①에서 꺼냄 / ②에 넣음
 *   [W1]   ②가 비어서💤 / ②에서 꺼냄 / read💤 / read⏰
 *
 * 빌드:  gcc -Wall -Wextra -pthread -o server server.c
 * 실행:  ./server            (포트 9000)
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define PORT 9000
#define WORKERS 2          /* 일부러 작게: 느린 클라이언트 2개면 다 묶인다 */
#define QSIZE 64

/* ---------- ② 일감 대기열: fd 번호를 순서대로 담는 배열 ---------- */
static int queue[QSIZE];
static int q_head = 0, q_count = 0;
static pthread_mutex_t q_lock = PTHREAD_MUTEX_INITIALIZER;   /* 몰라도 됨: 동시에 건드리지 않게 */
static pthread_cond_t  q_not_empty = PTHREAD_COND_INITIALIZER; /* 몰라도 됨: "비었으면 잠들기" 장치 */

static void queue_push(int fd)            /* main 이 부름 */
{
    pthread_mutex_lock(&q_lock);
    queue[(q_head + q_count) % QSIZE] = fd;
    q_count++;
    printf("[main] ②에 fd %d 넣음 (② 안에 %d개 대기 중)\n", fd, q_count);
    fflush(stdout);
    pthread_cond_signal(&q_not_empty);    /* 잠든 Worker 하나 깨우기 */
    pthread_mutex_unlock(&q_lock);
}

static int queue_pop(int id)              /* Worker 가 부름 */
{
    pthread_mutex_lock(&q_lock);
    while (q_count == 0) {
        printf("[W%d]   ②가 비어 있음 -> 일감 올 때까지 잠듦💤\n", id);
        fflush(stdout);
        pthread_cond_wait(&q_not_empty, &q_lock);
        printf("[W%d]   ⏰ 깨어남\n", id);
    }
    int fd = queue[q_head];
    q_head = (q_head + 1) % QSIZE;
    q_count--;
    printf("[W%d]   ②에서 fd %d 꺼냄 (② 안에 %d개 남음)\n", id, fd, q_count);
    fflush(stdout);
    pthread_mutex_unlock(&q_lock);
    return fd;
}

/* ---------- Worker: 영원히 "② 꺼내기 → 처리" 반복 ---------- */
static void *worker(void *arg)
{
    int id = (int)(long)arg;
    for (;;) {
        int fd = queue_pop(id);

        char buf[4096];
        printf("[W%d]   read(%d) -> 요청 데이터 올 때까지 잠듦💤\n", id, fd);
        fflush(stdout);
        ssize_t n = read(fd, buf, sizeof buf - 1);
        if (n <= 0) {
            printf("[W%d]   read(%d) = %zd -> 요청 없이 끝남, close\n", id, fd, n);
            close(fd);
            continue;
        }
        printf("[W%d]   ⏰ read(%d) 깨어남 -> %zd 바이트 (커널 -> 내 buf)\n", id, fd, n);

        const char *body = "hello from thread-pool server\n";
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
        printf("[W%d]   fd %d 응답 보내고 close -> 다시 ②로\n", id, fd);
        fflush(stdout);
    }
    return NULL;
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
    listen(listen_fd, 16);   /* ① 커널 대기열 생김 (커널 안) */
    printf("[main] 포트 %d 준비 완료. Worker %d개 만듦\n", PORT, WORKERS);
    fflush(stdout);

    /* Worker 를 처음에 딱 WORKERS 개만 만든다 */
    for (long i = 1; i <= WORKERS; i++) {
        pthread_t tid;
        pthread_create(&tid, NULL, worker, (void *)i);
        pthread_detach(tid);
    }
    usleep(100000);          /* Worker 들이 먼저 잠드는 로그가 보이게 잠깐 기다림 */

    for (;;) {
        printf("\n[main] accept() -> ①이 비어 있으면 잠듦💤\n");
        fflush(stdout);
        int conn_fd = accept(listen_fd, NULL, NULL);
        printf("[main] ⏰ ①에서 연결 꺼냄 -> fd %d 생김\n", conn_fd);
        queue_push(conn_fd);
    }
}
