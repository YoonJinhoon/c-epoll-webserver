#!/usr/bin/env bash
# 요구사항 파일-1, 파일-3: 루트 폴더의 파일을 보내고, 없으면 404
#
# 통과 기준:
#   ① GET /index.html   → 200, 본문에 "It works!"   (www/index.html)
#   ② GET /nothing.html → 404                       (www 에 없는 파일)
#
# 사용법:  bash tests/test_static.sh <서버 실행 파일>
#   예)    bash tests/test_static.sh ./webserver

SERVER="$1"
PORT=9000

if [ -z "$SERVER" ]; then
    echo "사용법: bash $0 <서버 실행 파일>"
    exit 2
fi

if nc -z localhost "$PORT" 2> /dev/null; then
    echo "[파일] 중단: 포트 $PORT 를 이미 다른 프로그램이 쓰고 있음. 켜져 있는 서버를 끄고 다시 실행"
    exit 2
fi

"$SERVER" "$PORT" www > /dev/null 2>&1 &
SERVER_PID=$!
sleep 0.3
if ! kill -0 "$SERVER_PID" 2> /dev/null; then
    echo "[파일] 중단: 서버가 바로 종료됨 ($SERVER)"
    exit 2
fi

CODE=0

# ① 있는 파일
S1=$(curl -s -o /tmp/static_body.$$ -w '%{http_code}' "localhost:$PORT/index.html")
if [[ "$S1" == "200" ]] && grep -q "It works!" /tmp/static_body.$$; then
    echo "[파일] ① /index.html   : PASS ✅  200 + It works!"
else
    echo "[파일] ① /index.html   : FAIL ❌  상태 $S1, 본문: $(head -c 80 /tmp/static_body.$$ 2> /dev/null)"
    CODE=1
fi

# ② 없는 파일
S2=$(curl -s -o /dev/null -w '%{http_code}' "localhost:$PORT/nothing.html")
if [[ "$S2" == "404" ]]; then
    echo "[파일] ② /nothing.html : PASS ✅  404"
else
    echo "[파일] ② /nothing.html : FAIL ❌  상태 $S2"
    CODE=1
fi

rm -f /tmp/static_body.$$
pkill -P $$ 2> /dev/null
wait 2> /dev/null
exit $CODE
