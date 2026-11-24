#!/usr/bin/env bash
# 요구사항 HTTP-4: POST 본문을 Content-Length 만큼 끝까지 읽는가
#
# 통과 기준 (POST /echo = 받은 본문을 그대로 돌려줌):
#   ① 머리 + "name=jin" 한 번에        → 응답 본문 name=jin
#   ② 머리 + "nam" 만                  → 응답 없음 (기다림)
#      이어서 "e=jin"                  → 응답 본문 name=jin
#
# 사용법:  bash tests/test_post_body.sh <서버 실행 파일>
#   예)    bash tests/test_post_body.sh ./webserver

SERVER="$1"
PORT=${PORT:-9000}  # 다른 포트: PORT=9100 bash ...

if [ -z "$SERVER" ]; then
    echo "사용법: bash $0 <서버 실행 파일>"
    exit 2
fi

if nc -z localhost "$PORT" 2> /dev/null; then
    echo "[POST] 중단: 포트 $PORT 를 이미 다른 프로그램이 쓰고 있음. 켜져 있는 서버를 끄고 다시 실행"
    exit 2
fi

"$SERVER" "$PORT" www > /dev/null 2>&1 &
SERVER_PID=$!
sleep 0.3
if ! kill -0 "$SERVER_PID" 2> /dev/null; then
    echo "[POST] 중단: 서버가 바로 종료됨 ($SERVER)"
    exit 2
fi

HEAD=$'POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 8\r\n\r\n'
CODE=0

# ① 한 번에
exec 3<> "/dev/tcp/localhost/$PORT"
printf '%s' "${HEAD}name=jin" >&3
R1=$(timeout 2 cat <&3 | tr -d '\r')
exec 3>&-
if [[ "$R1" == "HTTP/1.1 200"* && "$(echo "$R1" | tail -1)" == "name=jin" ]]; then
    echo "[POST] ① 한 번에        : PASS ✅  본문 name=jin"
else
    echo "[POST] ① 한 번에        : FAIL ❌  받은 응답: $(echo "$R1" | head -1) / 본문: $(echo "$R1" | tail -1)"
    CODE=1
fi

# ② 조각: 머리 + "nam" → (응답 없어야 함) → "e=jin"
exec 3<> "/dev/tcp/localhost/$PORT"
printf '%s' "${HEAD}nam" >&3
if read -r -t 1 EARLY <&3; then
    echo "[POST] ② 조각          : FAIL ❌  본문이 3바이트만 왔는데 응답이 옴: $EARLY"
    CODE=1
else
    printf 'e=jin' >&3
    R2=$(timeout 2 cat <&3 | tr -d '\r')
    if [[ "$R2" == "HTTP/1.1 200"* && "$(echo "$R2" | tail -1)" == "name=jin" ]]; then
        echo "[POST] ② 조각          : PASS ✅  nam 때는 응답 없음, e=jin 뒤에 name=jin"
    else
        echo "[POST] ② 조각          : FAIL ❌  받은 응답: $(echo "$R2" | head -1) / 본문: $(echo "$R2" | tail -1)"
        CODE=1
    fi
fi
exec 3>&-

pkill -P $$ 2> /dev/null
wait 2> /dev/null
exit $CODE
