# c-epoll-webserver

C와 POSIX 소켓 API만으로 만든 HTTP/1.1 웹 서버 + Reverse Proxy입니다. 크래프톤 정글 팀 9 과제로 만들었습니다.

- **동시성:** `epoll` 이벤트 루프, 스레드 1개
- **정적 파일:** MIME 타입, 404 / 403 / 500, 루트 밖 접근 차단
- **Reverse Proxy:** `/api/`로 시작하는 요청을 백엔드(8001)로 전달. 백엔드가 꺼져 있으면 502, 3초 안에 응답이 없으면 504
- **방어:** 30초 동안 조용한 연결은 닫음 (Slowloris 1차 방어), SIGPIPE 무시

모든 기능은 "주장 → 검증 방법 → 증거"를 실행 전에 적고, 직접 실험해서 확인했습니다. 기록: [docs/verify.md](docs/verify.md)

## 빌드 · 실행

Linux / WSL2 (Ubuntu)에서 빌드합니다.

```bash
make                    # ./webserver 빌드
./webserver 9100 www    # <port> <static_root>
```

Reverse Proxy를 확인하려면 백엔드를 하나 띄웁니다.

```bash
mkdir -p /tmp/b1/api && echo "8001 백엔드가 처리함" > /tmp/b1/api/hello
python3 -m http.server 8001 -d /tmp/b1     # 백엔드
curl -i localhost:9100/api/hello           # 우리 서버를 거쳐 백엔드 응답을 받음
curl -i localhost:9100/index.html          # 우리 서버가 직접 파일로 응답
```

## 기능과 검증 결과

| 영역 | 내용 | 확인 방법 | 결과 |
|---|---|---|---|
| TCP 소켓 | `socket → bind → listen → accept`, `SO_REUSEADDR` | 실험 01~04 | ✅ |
| 요청 끝 식별 | 요청이 여러 조각으로 와도 `\r\n\r\n`까지 모아서 처리 | `tests/test_split_request.sh` | ✅ |
| 요청 라인 파싱 | GET / POST, 형식 오류 400, 그 밖의 메서드 501 | `tests/test_request_line.sh`, `make test` | ✅ |
| POST 본문 | `Content-Length`만큼 끝까지 읽기 (조각나서 와도) | `tests/test_post_body.sh` | ✅ |
| 정적 파일 | 파일 응답, 404, MIME 타입 (500은 코드만 있고 미검증) | `tests/test_static.sh` | ✅ |
| 보안 | `/../src/main.c` 같은 루트 밖 접근 → 403 | `tests/test_static.sh` | ✅ |
| 동시 처리 | 느린 클라이언트가 붙어 있어도 다른 클라이언트가 응답받음 | `tests/test_slow_client.sh` | ✅ |
| Idle timeout | 느린 클라이언트 1,100개로 연결 칸(1,019개)을 채움 → 30초 뒤 전부 닫히고 새 요청 200 | 터미널 3개 수동 실험 | ✅ |
| Reverse Proxy | `/api/*` → 8001 전달, 백엔드 응답을 그대로 돌려줌 | curl | ✅ |
| 백엔드 장애 | 백엔드 꺼짐 → 502 / 대답 없음 → 3초 뒤 504, 다른 요청은 계속 처리 | `nc -l 8001`로 대답 안 하는 백엔드 | ✅ |

## 테스트

```bash
make test                                   # 단위 테스트 (요청 파서, MIME)
bash tests/test_static.sh ./webserver       # 기능 테스트 (서버를 띄우고 curl로 검사)
bash tests/test_slow_client.sh ./webserver 2  # 느린 클라이언트 2개가 붙어 있어도 응답하는지
make asan                                   # AddressSanitizer / LeakSanitizer / UBSan 빌드로 단위 테스트
```

## 설계

```
curl ──▶ [ main 스레드: epoll_wait ] ──┬─ listen fd → accept → conn_open (보관함 + epoll 등록)
                                       ├─ 연결 fd  → conn_on_data (조각 모으기 → 파싱 → 응답)
                                       │              ├─ /api/*  → proxy_to_backend → 8001
                                       │              └─ 그 밖   → serve_file (www/)
                                       └─ 1초마다 → 30초 넘게 조용한 연결 닫기
```

- 연결마다 보관함(`struct conn`: fd, 받은 바이트, 마지막 도착 시각)을 두고, 요청이 조각나서 와도 이어 붙입니다.
- `epoll_wait`는 1초 timeout으로 깨어나서, 조용한 연결을 찾아 닫습니다. 조용한 연결은 `events`에 나오지 않으므로 보관함 전체를 훑습니다.

### 동시성 모델을 고른 과정 (`experiments/`)

| 실험 | 방식 | 느린 클라이언트가 있을 때 |
|---|---|---|
| 01 | 단일 스레드 | 서버 전체가 멈춤 |
| 02 | 연결마다 스레드 | 안 멈춤. 대신 연결 수만큼 스레드가 생김 |
| 03 | 스레드 풀 (Worker 2개) | 느린 클라이언트 2개면 멈춤 |
| 04 | epoll (스레드 1개) | 안 멈춤 → **src/에서 채택** |

## 한계와 다음 단계

알고 있지만 아직 해결하지 않은 것입니다.

- **느린 백엔드:** 백엔드 쪽 소켓을 epoll에 등록하지 않고 바로 `read`합니다. 그래서 대답 없는 백엔드가 있으면 서버 전체가 최대 3초 멈춥니다. 해결하려면 백엔드 fd도 epoll에 등록해야 합니다.
- **Slowloris 변형:** 29초마다 1바이트씩 보내는 연결은 닫지 못합니다. 전체 마감 시간이나 최소 전송 속도 규칙이 필요합니다.
- **헤더:** `Content-Length`만 읽고, `Host`와 `Connection`은 읽지 않습니다. 응답은 항상 `Connection: close`입니다 (keep-alive 없음).
- **라우팅:** 백엔드가 8001 하나이고 코드에 고정되어 있습니다. 여러 백엔드, CLI 라우팅 규칙, Round-Robin은 아직 없습니다.
- **큰 요청:** 머리가 8192바이트를 넘으면 응답 없이 연결을 끊습니다 (431 응답 없음).
- **종료:** SIGINT / SIGTERM을 받으면 정리 없이 바로 종료됩니다.

## 팀

크래프톤 정글 팀 9. 초기 프로젝트 세팅(Makefile, CI, Dev Container)은 팀원이 작성했고, 동시성 실험과 `src/` 서버는 [@YoonJinhoon](https://github.com/YoonJinhoon)이 작성했습니다.

| GitHub |
|---|
| [@cc8s](https://github.com/cc8s) |
| [@bennjung](https://github.com/bennjung) |
| [@YoonJinhoon](https://github.com/YoonJinhoon) |
