# 검증 기록

규칙: 주장과 방법은 **실행 전에** 적는다. 증거는 복붙할 수 있는 것만. 판정자를 적는다 (AI가 판정한 건 아직 내 검증이 아니다).

| 날짜 | 요구사항 | 주장 (통과 기준) | 검증 방법 | 증거 | 판정자 |
|---|---|---|---|---|---|
| 11/20 | 연결-2 | B가 가만히 있어도 C가 응답받는다 | 실험 02, 터미널 A/B/C 손으로 | `[T2] [7]` 있음 + `[T1] [6]` 없음 | 진훈 |
| 11/21 | 연결-2 | B가 가만히 있을 때, C가 **2초 안에** 서버 응답을 받으면 PASS | `bash tests/test_slow_client.sh <서버>` | 01: `FAIL ❌` / 02: `PASS ✅` (기대와 일치) | 진훈 |

| 11/21 | 연결-2 | (스레드 풀, Worker 2개) 느린 클라이언트 1개면 PASS — W1이 B를, W2가 C를 맡으니까 | `bash tests/test_slow_client.sh experiments/03-thread-pool/server` | `PASS ✅` (예측과 일치) | 진훈 |
| 11/21 | 연결-3 | (스레드 풀, Worker 2개) 느린 클라이언트 2개면 C는 연결은 되지만 처리 안 됨 | 실험 03, 터미널 A/B1/B2/C 손으로 | `[main] ⏰ ... fd 6 생김` 있음 + `②에서 fd 6 꺼냄` 없음 → B1이 요청 보내자 `[W1] ②에서 fd 6 꺼냄` | 진훈 |

| 11/23 | 연결-3 | (스레드 풀, Worker 2개) 느린 클라이언트 2개면 FAIL — Worker 2개가 다 묶여서 C를 처리할 Worker가 없으니까 | `bash tests/test_slow_client.sh experiments/03-thread-pool/server 2` | `FAIL ❌ 느린 클라이언트 2개 때문에...` (예측과 일치) | 진훈 |

| 11/23 | 연결-3 | (epoll, 스레드 1개) 느린 클라이언트 2개여도 PASS — 데이터가 온 fd만 골라서 read하니까 | 손으로 A/B1/B2/C → `bash tests/test_slow_client.sh experiments/04-epoll/server 2` | 로그에 `⏰ fd 7에 요청 데이터 도착`만 있고 fd 5·6은 없음 / `PASS ✅` (예측과 일치) | 진훈 |

| 11/23 | HTTP-2 | 요청을 두 조각으로 보낼 때, 첫 조각만 보낸 시점엔 응답이 없고 빈 줄까지 보낸 뒤 응답이 온다 | `bash tests/test_split_request.sh <서버>` | S1 `./webserver`: `PASS ✅` / 실험 04: `FAIL ❌ 첫 조각만 보냈는데 응답이 옴` (예측: 둘 다 PASS → 04는 틀림. 이유: 04는 \r\n\r\n을 찾지 않고 read 한 번에 응답) | 진훈 |
| 11/23 | HTTP-2 | request_head_end가 머리 끝을 맞게 찾는다 (조각, \n만, 본문 붙음 등 7경우) | `make test` (tests/test_request.c) | 7개 PASS | Claude (진훈 미확인) |

| 11/23 | HTTP-1 ① | `GET /hello HTTP/1.1` → 200, 본문에 method=GET / path=/hello / version=HTTP/1.1 (통째로 돌려주기와 구분되게 이름표) | 손으로: `./webserver 9000 www` + `curl localhost:9000/hello` | 본문 3줄 일치, 로그 `파싱 성공: method=GET path=/hello version=HTTP/1.1 -> 200` (조각 1번에 끝남) | 진훈 |

| 11/23 | HTTP-1 ② | `/hello HTTP/1.1 GET /hello HTTP/1.1` (3조각 아님) → 400 | `bash tests/test_request_line.sh ./webserver` | ① PASS, ② `PASS ✅ 400` (예측과 일치) | 진훈 |

| 11/23 | HTTP-1 ③④ | (진훈이 직접 쓴 첫 코드) GET/POST가 아닌 메서드 → 501, POST → 200 | `bash tests/test_request_line.sh ./webserver` (③④ 추가) | ①②③④ 모두 `PASS ✅` (예측과 일치). 과정: else if 두 개로 나눔 → ① FAIL로 잡힘 → `&&` 하나로 합침 | 진훈 |

| 11/24 | 파일-1·3 | `/index.html` → 200 + It works! / `/nothing.html` → 404 | 손으로 `curl -i localhost:9000/nothing.html` + `bash tests/test_static.sh ./webserver` | `HTTP/1.1 404 Not Found`, 로그 `파일 www/nothing.html 없음 -> 404` / ①② `PASS ✅` (예측과 일치) | 진훈 |

| 11/24 | 파일-4 (보안) | **버그 확인:** `GET /../src/main.c` → www 밖의 소스 코드가 나온다 (경로 탈출) | `curl --path-as-is localhost:9000/../src/main.c` | 예측 "main.c 내용이 나온다" → 실제로 나옴 ❌ (S3-c에서 403으로 고칠 것) | 진훈 |

| 11/24 | 파일-4 (보안) | `GET /../src/main.c` → 403, 소스 코드 안 나옴 | 새 테스트 ③이 **고치기 전 코드에서 FAIL** 확인(Claude) → 진훈이 손으로 `curl -i --path-as-is localhost:9000/../src/main.c` | `HTTP/1.1 403 Forbidden`, 로그 `경로 /../src/main.c 에 ".." 있음 -> 403` (예측과 일치) | 진훈 |

| 11/24 | 파일-2 (MIME) | (진훈 작성 `mime_type`) index.html → `Content-Type: text/html`, 크롬에서 웹페이지로 보임 | `curl -i localhost:9000/index.html` + 크롬 / `make test` 9개 / test_static ④ | `Content-Type: text/html`, 크롬에 큰 글씨 It works! (예측: "It works! 글자만" → 일치, 굵기는 예상 못 함) | 진훈 |

| 11/24 | HTTP-4 | (진훈 작성: 본문 다 모였나 조건) `POST /echo` Content-Length 8: 한 번에 → name=jin / `nam`만 → 응답 없음, `e=jin` 뒤 → name=jin | `bash tests/test_post_body.sh ./webserver` (진훈 코드 전: ①② FAIL 확인) | 1차 `clen != c->len + head` → 둘 다 응답 없음 FAIL (숫자 넣어 따라가서 원인 찾음: 8 != 132 참 → 영원히 기다림) → `c->len < head + (size_t)clen` → ①② PASS | 진훈 |
| 11/24 | 자원 (S6-a, timeout 전) | 느린 클라이언트(nc) 1,100개 + curl 1개 → C가 응답을 못 받아야 함 | 터미널 3개 수동: `./webserver 9100 www \| tee /tmp/s6.log` / bash에서 nc 1,100개 / `curl -v`, `grep -c` | `pgrep -c nc` 1100, `연결됨` **1019** (예측 1019 ✅), `꽉 참` **82** (= 81 + curl, 전부 fd 1024), curl `(56) Connection reset by peer` → **C 거절 확인** | 진훈 |

## 테스트의 한계 기록
| 날짜 | 테스트 | 증명 못 하는 것 |
|---|---|---|
| 11/21 | test_slow_client.sh | 느린 클라이언트를 **1개만** 붙임 → Worker 2개짜리 스레드 풀의 약점(느린 클라이언트 2개면 막힘)은 못 잡음 → **11/23 해결**: 개수 인자 추가 |

| 11/23 | test_request_line.sh ①② | 501 기능을 추가해도 ①②는 원래 PASS → 새 기능을 검사하지 않음 → ③ HELLO→501, ④ POST→200 추가 |

## 테스트가 거짓말한 기록
| 날짜 | 테스트 | 무슨 일 | 고친 것 |
|---|---|---|---|
| 11/21 | test_slow_client.sh | 실험 02 서버가 켜져 있어서, 실험 01 검사인데 PASS가 나옴 (엉뚱한 서버를 검사) | 포트 사용 중이면 중단 / 서버가 바로 죽으면 중단 |
| 11/23 | test_request_line.sh | `make`가 컴파일 에러로 실패했는데 테스트를 돌림 → **옛날 ./webserver**를 검사함 | 규칙: make가 에러 없이 끝났을 때만 테스트 |
| 11/24 | S6 수동 실험 | 터미널 셸이 **fish**라서 bash 문법 `for ...; do ...; done`이 실행되지 않음 → 느린 클라이언트 0개인 채로 curl 200 (실험이 아무것도 안 함) | `pgrep -c nc`로 개수를 먼저 확인 / B 터미널에서 `bash` 입력 후 실행 |
