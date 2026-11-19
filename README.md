# Team 9 Web Server

크래프톤 정글 WEEK13 — C 와 POSIX 소켓 API 만으로 만드는 HTTP 웹 서버 + Proxy / Reverse Proxy.

> 🚧 구현 중

## 기능

| 영역 | 내용 | 상태 |
|---|---|---|
| TCP 소켓 서버 | `socket → bind → listen → accept` | ⬜ |
| HTTP 요청 파싱 | 요청 라인, 헤더, 본문 (GET / POST) | ⬜ |
| HTTP 응답 생성 | 상태 라인, 표준 헤더, 본문 | ⬜ |
| 정적 파일 서빙 | 루트 디렉토리, MIME 매핑, 404 / 403 / 500 | ⬜ |
| 동적 요청 처리 | | ⬜ |
| 동시 요청 처리 | | ⬜ |
| Proxy / Reverse Proxy | | ⬜ |
| 종료 처리 | SIGINT / SIGTERM, SIGPIPE 무시 | ⬜ |

## 빌드 · 실행

Linux / WSL2 / Dev Container(`.devcontainer/`, Ubuntu 22.04) 에서 빌드합니다.

```bash
make                 # ./webserver 빌드
make run             # PORT=8080 ROOT=www 기본값으로 실행
./webserver 8080 www # 직접 실행: <port> <static_root>
```

## 테스트

```bash
make test   # 단위 테스트
make asan   # AddressSanitizer · LeakSanitizer · UBSan 빌드로 단위 테스트
```

## 설계

<!-- 동시성 모델, Proxy 방식, 모듈 구조 -->

## 팀

| 이름 | GitHub |
|---|---|
| | [@cc8s](https://github.com/cc8s) |
| | [@bennjung](https://github.com/bennjung) |
| | [@YoonJinhoon](https://github.com/YoonJinhoon) |
