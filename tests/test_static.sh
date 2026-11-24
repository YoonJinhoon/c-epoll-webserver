#!/usr/bin/env bash
# 요구사항 파일-1, 파일-3: 루트 폴더의 파일을 보내고, 없으면 404
#
# 통과 기준:
#   ① GET /index.html   → 200, 본문에 "It works!"   (www/index.html)
#   ② GET /nothing.html → 404                       (www 에 없는 파일)
#   ③ GET /../src/main.c → 403, 소스 코드 내용은 나오지 않음 (경로 탈출 막기, S3-c)
#   ④ GET /index.html 의 Content-Type → text/html (MIME, S3-b)
#
# 사용법:  bash tests/test_static.sh <서버 실행 파일>
#   예)    bash tests/test_static.sh ./webserver

SERVER="$1"
PORT=${PORT:-9000}  # 다른 포트: PORT=9100 bash ...

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

# ④ MIME 타입 (S3-b): html 파일은 Content-Type: text/html 로 와야 한다
CT=$(curl -s -o /dev/null -w '%{content_type}' "localhost:$PORT/index.html")
if [[ "$CT" == "text/html" ]]; then
    echo "[파일] ④ Content-Type   : PASS ✅  text/html"
else
    echo "[파일] ④ Content-Type   : FAIL ❌  $CT"
    CODE=1
fi

# ③ 경로 탈출 시도 (S3-c): 403 이 오고, 소스 코드 내용은 나오면 안 된다
S3=$(curl -s --path-as-is -o /tmp/static_body.$$ -w '%{http_code}' "localhost:$PORT/../src/main.c")
if [[ "$S3" == "403" ]] && ! grep -q "#include" /tmp/static_body.$$; then
    echo "[파일] ③ /../src/main.c : PASS ✅  403, 소스 코드 안 나옴"
else
    echo "[파일] ③ /../src/main.c : FAIL ❌  상태 $S3, 본문 첫 줄: $(head -1 /tmp/static_body.$$ 2> /dev/null)"
    CODE=1
fi

rm -f /tmp/static_body.$$
pkill -P $$ 2> /dev/null
wait 2> /dev/null
exit $CODE
