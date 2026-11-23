#!/usr/bin/env bash
# 요구사항 HTTP-2: 요청이 조각나서 와도, 머리 끝(빈 줄)까지 다 온 뒤에 응답하는가
#
# 통과 기준: 요청을 두 조각으로 나눠 보낼 때,
#   ① 첫 조각만 보낸 시점에는 응답이 없고
#   ② 빈 줄까지 보낸 뒤에는 응답이 온다
#
# 사용법:  bash tests/test_split_request.sh <서버 실행 파일>
#   예)    bash tests/test_split_request.sh ./webserver                         → PASS 여야 정상
#          bash tests/test_split_request.sh experiments/04-epoll/server         → FAIL 이어야 정상 (첫 조각에 바로 응답)

SERVER="$1"
PORT=9000
WAIT=1         # 초. 첫 조각 뒤에 응답이 오는지 이만큼 지켜본다

if [ -z "$SERVER" ]; then
    echo "사용법: bash $0 <서버 실행 파일>"
    exit 2
fi

# 안전장치 1: 포트가 이미 쓰이고 있으면 중단
if nc -z localhost "$PORT" 2> /dev/null; then
    echo "[HTTP-2] 중단: 포트 $PORT 를 이미 다른 프로그램이 쓰고 있음. 켜져 있는 서버를 끄고 다시 실행"
    exit 2
fi

# 서버 켜기 (인자: 포트, 정적 파일 폴더. 실험 서버들은 인자를 무시한다)
"$SERVER" "$PORT" www > /dev/null 2>&1 &
SERVER_PID=$!
sleep 0.3

# 안전장치 2: 서버가 바로 죽었으면 중단
if ! kill -0 "$SERVER_PID" 2> /dev/null; then
    echo "[HTTP-2] 중단: 서버가 바로 종료됨 ($SERVER)"
    exit 2
fi

# 서버에 연결 (bash 기능: fd 3 번으로 TCP 연결을 연다)
exec 3<> "/dev/tcp/localhost/$PORT"

# 첫 조각: 요청 라인 중간까지만
printf 'GET / HT' >&3
if read -r -t "$WAIT" FIRST <&3; then
    RESULT="FAIL ❌  첫 조각('GET / HT')만 보냈는데 응답이 옴: $FIRST"
    CODE=1
else
    # 둘째 조각: 나머지 + 빈 줄
    printf 'TP/1.1\r\nHost: localhost\r\n\r\n' >&3
    if read -r -t 2 LINE <&3 && [[ "$LINE" == HTTP/1.1* ]]; then
        RESULT="PASS ✅  첫 조각엔 응답 없음, 빈 줄까지 보낸 뒤 응답 옴 ($(echo "$LINE" | tr -d '\r'))"
        CODE=0
    else
        RESULT="FAIL ❌  빈 줄까지 보냈는데 2초 안에 응답이 없음"
        CODE=1
    fi
fi

# 정리
exec 3>&-
pkill -P $$ 2> /dev/null
wait 2> /dev/null

echo "[HTTP-2] $SERVER : $RESULT"
exit $CODE
