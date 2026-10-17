#!/usr/bin/env bash
set -euo pipefail

SKRUMI_BIN="${1:-./skrumi}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVER_SCRIPT="$SCRIPT_DIR/server_fixture.skm"
PORT=18095

cleanup() {
    if [[ -n "${SERVER_PID:-}" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

# 1. Start server in background
"$SKRUMI_BIN" "$SERVER_SCRIPT" >/dev/null 2>&1 &
SERVER_PID=$!

# Wait for server to start listening
READY=0
for i in $(seq 1 30); do
    if curl -s -m 1 "http://127.0.0.1:$PORT/test/get" >/dev/null 2>&1; then
        READY=1
        break
    fi
    sleep 0.1
done

if [[ "$READY" -ne 1 ]]; then
    echo "FAIL: Server failed to start on port $PORT"
    exit 1
fi

# 2. Test GET route, method, path, response status
GET_STATUS=$(curl -s -o /tmp/skrumi_get.out -w "%{http_code}" "http://127.0.0.1:$PORT/test/get")
if [[ "$GET_STATUS" != "200" ]]; then
    echo "FAIL: Expected GET status 200, got $GET_STATUS"
    exit 1
fi

grep -q '"route":"get_ok"' /tmp/skrumi_get.out || { echo "FAIL: GET route not matched"; exit 1; }
grep -q '"path":"/test/get"' /tmp/skrumi_get.out || { echo "FAIL: GET path not matched"; exit 1; }
grep -q '"method":"GET"' /tmp/skrumi_get.out || { echo "FAIL: GET method not matched"; exit 1; }

# 3. Test POST route, request body, response status
POST_DATA='{"msg":"hello_from_skrumi_test"}'
POST_STATUS=$(curl -s -o /tmp/skrumi_post.out -w "%{http_code}" -X POST -H "Content-Type: application/json" -d "$POST_DATA" "http://127.0.0.1:$PORT/test/post")
if [[ "$POST_STATUS" != "201" ]]; then
    echo "FAIL: Expected POST status 201, got $POST_STATUS"
    exit 1
fi

grep -q '"route":"post_ok"' /tmp/skrumi_post.out || { echo "FAIL: POST route not matched"; exit 1; }
grep -q 'hello_from_skrumi_test' /tmp/skrumi_post.out || { echo "FAIL: POST body not matched"; exit 1; }

# 4. Test Telegram-style Webhook route with Unicode/emoji and json_path
TG_PAYLOAD='{"update_id":123,"message":{"message_id":1,"from":{"id":42,"first_name":"Алексей 🦊"},"chat":{"id":"987654"},"text":"/ping 🦊"}}'
TG_STATUS=$(curl -s -o /tmp/skrumi_tg.out -w "%{http_code}" -X POST -H "Content-Type: application/json; charset=utf-8" -d "$TG_PAYLOAD" "http://127.0.0.1:$PORT/telegram")
if [[ "$TG_STATUS" != "200" ]]; then
    echo "FAIL: Expected Telegram webhook status 200, got $TG_STATUS"
    exit 1
fi

grep -q '"ok":true' /tmp/skrumi_tg.out || { echo "FAIL: Telegram response ok flag missing"; exit 1; }
grep -q '"chat_id":"987654"' /tmp/skrumi_tg.out || { echo "FAIL: Telegram chat_id not matched"; exit 1; }
grep -q 'Привет, Алексей 🦊' /tmp/skrumi_tg.out || { echo "FAIL: Telegram response Cyrillic/emoji not matched"; exit 1; }

# 5. Test Skrumi HTTP client with http_post_json against the local server
CLIENT_SCRIPT="/tmp/skrumi_client_test_$$.skm"
cat << 'EOF' > "$CLIENT_SCRIPT"
using http;
using json;

void main() {
    string payload = "{\"update_id\":999,\"message\":{\"chat\":{\"id\":\"55555\"},\"from\":{\"first_name\":\"Клиент\"},\"text\":\"Тест http_post_json\"}}";
    string resp = http_post_json("http://127.0.0.1:18095/telegram", payload);
    string ok = json_path(resp, "ok");
    string chat = json_path(resp, "chat_id");
    if (ok != "true" || chat != "55555") {
        print("FAIL_CLIENT_RESPONSE: " + resp);
    } else {
        print("CLIENT_HTTP_POST_JSON_OK");
    }
}
main();
EOF

CLIENT_OUT=$("$SKRUMI_BIN" "$CLIENT_SCRIPT")
rm -f "$CLIENT_SCRIPT"
if ! echo "$CLIENT_OUT" | grep -q "CLIENT_HTTP_POST_JSON_OK"; then
    echo "FAIL: http_post_json failed, output: $CLIENT_OUT"
    exit 1
fi

# 6. Test 404 for unknown route
NOT_FOUND_STATUS=$(curl -s -o /dev/null -w "%{http_code}" "http://127.0.0.1:$PORT/unknown_route")
if [[ "$NOT_FOUND_STATUS" != "404" ]]; then
    echo "FAIL: Expected 404 for unknown route, got $NOT_FOUND_STATUS"
    exit 1
fi

# 7. Stop server gracefully via /stop
STOP_RESP=$(curl -s "http://127.0.0.1:$PORT/stop")
if ! echo "$STOP_RESP" | grep -q '"stopped":true'; then
    echo "FAIL: Server stop response invalid: $STOP_RESP"
    exit 1
fi

# 8. Verify server process exits cleanly with code 0
wait "$SERVER_PID"
EXIT_CODE=$?
if [[ "$EXIT_CODE" -ne 0 ]]; then
    echo "FAIL: Server exited with non-zero code $EXIT_CODE"
    exit 1
fi

unset SERVER_PID

echo "HTTP_LOCAL_TEST_OK"
exit 0
