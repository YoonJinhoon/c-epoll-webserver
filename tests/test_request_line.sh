#!/usr/bin/env bash
# 요구사항 HTTP-1: 요청 라인을 세 조각(메서드, 경로, 버전)으로 쪼개는가
#
# 통과 기준:
#   ① POST /hello HTTP/1.1                   → 200, 본문에 method=POST / path=/hello / version=HTTP/1.1
#      (S2 때는 GET 으로 검사했지만, S3부터 GET 은 파일을 보내므로 POST 로 바꿈)
#   ② /hello HTTP/1.1 GET /hello HTTP/1.1     → 400 (3조각이 아님)
#   ③ HELLO /hello HTTP/1.1                  → 501 (모양은 맞지만 지원 안 하는 메서드)
#   ④ POST /hello HTTP/1.1                   → 200 (POST 는 지원)
#
# 사용법:  bash tests/test_request_line.sh <서버 실행 파일>
#   예)    bash tests/test_request_line.sh ./webserver

SERVER="$1"
PORT=9000

if [ -z "$SERVER" ]; then
    echo "사용법: bash $0 <서버 실행 파일>"
    exit 2
fi

if nc -z localhost "$PORT" 2> /dev/null; then
    echo "[HTTP-1] 중단: 포트 $PORT 를 이미 다른 프로그램이 쓰고 있음. 켜져 있는 서버를 끄고 다시 실행"
    exit 2
fi

"$SERVER" "$PORT" www > /dev/null 2>&1 &
SERVER_PID=$!
sleep 0.3
if ! kill -0 "$SERVER_PID" 2> /dev/null; then
    echo "[HTTP-1] 중단: 서버가 바로 종료됨 ($SERVER)"
    exit 2
fi

# 요청을 보내고 응답 전체를 받아오는 함수 (bash 의 /dev/tcp 로 직접 보냄)
send_raw() {
    exec 3<> "/dev/tcp/localhost/$PORT"
    printf '%s' "$1" >&3
    timeout 2 cat <&3
    exec 3>&-
}

CODE=0

# ① 정상 요청 (S3부터 GET 은 파일을 보내므로, 파싱 결과는 POST 로 확인한다)
R1=$(send_raw $'POST /hello HTTP/1.1\r\nHost: localhost\r\n\r\n' | tr -d '\r')
if [[ "$R1" == "HTTP/1.1 200"* && "$R1" == *"method=POST"* && "$R1" == *"path=/hello"* && "$R1" == *"version=HTTP/1.1"* ]]; then
    echo "[HTTP-1] ① 정상 요청 : PASS ✅  200 + method/path/version 따로 꺼냄"
else
    echo "[HTTP-1] ① 정상 요청 : FAIL ❌  받은 응답: $(echo "$R1" | head -1) / 본문: $(echo "$R1" | tail -3 | tr '\n' ' ')"
    CODE=1
fi

# ② 엉터리 요청 (3조각이 아님)
R2=$(send_raw $'/hello HTTP/1.1 GET /hello HTTP/1.1\r\nHost: localhost\r\n\r\n' | tr -d '\r')
if [[ "$R2" == "HTTP/1.1 400"* ]]; then
    echo "[HTTP-1] ② 엉터리 요청 : PASS ✅  400"
else
    echo "[HTTP-1] ② 엉터리 요청 : FAIL ❌  받은 응답: $(echo "$R2" | head -1)"
    CODE=1
fi

# ③ 지원하지 않는 메서드 (모양은 맞지만 GET/POST 가 아님)
R3=$(send_raw $'HELLO /hello HTTP/1.1\r\nHost: localhost\r\n\r\n' | tr -d '\r')
if [[ "$R3" == "HTTP/1.1 501"* ]]; then
    echo "[HTTP-1] ③ HELLO 메서드 : PASS ✅  501"
else
    echo "[HTTP-1] ③ HELLO 메서드 : FAIL ❌  받은 응답: $(echo "$R3" | head -1)"
    CODE=1
fi

# ④ POST 는 지원해야 함 (501 로 막으면 안 됨)
R4=$(send_raw $'POST /hello HTTP/1.1\r\nHost: localhost\r\n\r\n' | tr -d '\r')
if [[ "$R4" == "HTTP/1.1 200"* && "$R4" == *"method=POST"* ]]; then
    echo "[HTTP-1] ④ POST 메서드  : PASS ✅  200"
else
    echo "[HTTP-1] ④ POST 메서드  : FAIL ❌  받은 응답: $(echo "$R4" | head -1)"
    CODE=1
fi

pkill -P $$ 2> /dev/null
wait 2> /dev/null
exit $CODE
