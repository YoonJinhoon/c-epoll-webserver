#!/usr/bin/env bash
# 요구사항 연결-2: 느린 클라이언트가 다른 클라이언트를 막지 않는가
#
# 통과 기준: B가 가만히 있을 때, C가 LIMIT초 안에 서버의 응답을 받으면 PASS
#
# 사용법:  bash tests/test_slow_client.sh <서버 실행 파일>
#   예)    bash tests/test_slow_client.sh experiments/01-single-thread/server   → FAIL 이어야 정상
#          bash tests/test_slow_client.sh experiments/02-thread-per-conn/server → PASS 여야 정상

SERVER="$1"
PORT=9000
LIMIT=2        # 초. 정상 응답은 0.01초 안팎이라 넉넉하게 잡음

if [ -z "$SERVER" ]; then
    echo "사용법: bash $0 <서버 실행 파일>"
    exit 2
fi

# 안전장치 1: 포트가 이미 쓰이고 있으면 중단
#   (다른 서버가 켜져 있으면, 테스트 대상이 아니라 그 서버를 검사하게 된다)
if curl -s --max-time 1 "localhost:$PORT" > /dev/null 2>&1 || nc -z localhost "$PORT" 2> /dev/null; then
    echo "[연결-2] 중단: 포트 $PORT 를 이미 다른 프로그램이 쓰고 있음. 켜져 있는 서버를 끄고 다시 실행"
    exit 2
fi

# 터미널 A 역할: 서버 켜기 (로그는 버림)
"$SERVER" > /dev/null 2>&1 &
SERVER_PID=$!
sleep 0.3      # 서버가 listen 할 때까지 잠깐 기다림

# 안전장치 2: 서버가 켜지자마자 죽었으면 중단 (bind 실패 등)
if ! kill -0 "$SERVER_PID" 2> /dev/null; then
    echo "[연결-2] 중단: 서버가 바로 종료됨 ($SERVER)"
    exit 2
fi

# 터미널 B 역할: 연결만 하고 아무것도 안 보냄
#   (sleep 이 nc 의 입력을 붙잡고 있어서, nc 는 보낼 데이터 없이 연결만 유지한다)
sleep 30 | nc localhost "$PORT" > /dev/null &
sleep 0.3

# 터미널 C 역할: 요청 보내고 LIMIT초까지만 기다림
if curl -s --max-time "$LIMIT" "localhost:$PORT" > /dev/null; then
    RESULT="PASS ✅  C가 ${LIMIT}초 안에 응답 받음 (B가 막지 않음)"
    CODE=0
else
    RESULT="FAIL ❌  C가 ${LIMIT}초 안에 응답 못 받음 (B가 막음)"
    CODE=1
fi

# 정리: 이 스크립트가 띄운 것(서버, nc, sleep) 모두 끄기
pkill -P $$ 2> /dev/null
wait 2> /dev/null

echo "[연결-2] $SERVER : $RESULT"
exit $CODE
