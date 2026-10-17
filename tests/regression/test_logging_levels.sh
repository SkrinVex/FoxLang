#!/usr/bin/env bash
set -euo pipefail

SKRUMI_BIN="${1:-./skrumi}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_SCRIPT="$SCRIPT_DIR/logging_script.skm"
OUT="/tmp/skrumi_log_test.out"

# 1. SKRUMI_LOG_LEVEL=debug -> all messages
SKRUMI_LOG_LEVEL=debug "$SKRUMI_BIN" "$LOG_SCRIPT" > "$OUT" 2>&1
grep -q '\[DEBUG\] test-debug-msg' "$OUT" || { echo "FAIL: debug log missing"; exit 1; }
grep -q '\[INFO\] test-info-msg' "$OUT" || { echo "FAIL: info log missing in debug"; exit 1; }
grep -q '\[WARN\] test-warn-msg' "$OUT" || { echo "FAIL: warn log missing in debug"; exit 1; }
grep -q '\[ERROR\] test-error-msg' "$OUT" || { echo "FAIL: error log missing in debug"; exit 1; }

# 2. SKRUMI_LOG_LEVEL=info -> info, warn, error
SKRUMI_LOG_LEVEL=info "$SKRUMI_BIN" "$LOG_SCRIPT" > "$OUT" 2>&1
! grep -q '\[DEBUG\] test-debug-msg' "$OUT" || { echo "FAIL: debug log should be suppressed in info level"; exit 1; }
grep -q '\[INFO\] test-info-msg' "$OUT" || { echo "FAIL: info log missing"; exit 1; }
grep -q '\[WARN\] test-warn-msg' "$OUT" || { echo "FAIL: warn log missing"; exit 1; }
grep -q '\[ERROR\] test-error-msg' "$OUT" || { echo "FAIL: error log missing"; exit 1; }

# 3. SKRUMI_LOG_LEVEL=warn -> warn, error
SKRUMI_LOG_LEVEL=warn "$SKRUMI_BIN" "$LOG_SCRIPT" > "$OUT" 2>&1
! grep -q '\[DEBUG\] test-debug-msg' "$OUT" || { echo "FAIL: debug log should be suppressed in warn level"; exit 1; }
! grep -q '\[INFO\] test-info-msg' "$OUT" || { echo "FAIL: info log should be suppressed in warn level"; exit 1; }
grep -q '\[WARN\] test-warn-msg' "$OUT" || { echo "FAIL: warn log missing in warn level"; exit 1; }
grep -q '\[ERROR\] test-error-msg' "$OUT" || { echo "FAIL: error log missing in warn level"; exit 1; }

# 4. SKRUMI_LOG_LEVEL=error -> only error
SKRUMI_LOG_LEVEL=error "$SKRUMI_BIN" "$LOG_SCRIPT" > "$OUT" 2>&1
! grep -q '\[DEBUG\] test-debug-msg' "$OUT" || { echo "FAIL: debug log should be suppressed in error level"; exit 1; }
! grep -q '\[INFO\] test-info-msg' "$OUT" || { echo "FAIL: info log should be suppressed in error level"; exit 1; }
! grep -q '\[WARN\] test-warn-msg' "$OUT" || { echo "FAIL: warn log should be suppressed in error level"; exit 1; }
grep -q '\[ERROR\] test-error-msg' "$OUT" || { echo "FAIL: error log missing in error level"; exit 1; }

# 5. SKRUMI_LOG_LEVEL=off -> nothing
SKRUMI_LOG_LEVEL=off "$SKRUMI_BIN" "$LOG_SCRIPT" > "$OUT" 2>&1
! grep -q '\[DEBUG\]' "$OUT" || { echo "FAIL: log off leaked debug"; exit 1; }
! grep -q '\[INFO\]' "$OUT" || { echo "FAIL: log off leaked info"; exit 1; }
! grep -q '\[WARN\]' "$OUT" || { echo "FAIL: log off leaked warn"; exit 1; }
! grep -q '\[ERROR\]' "$OUT" || { echo "FAIL: log off leaked error"; exit 1; }

# 6. SKRUMI_LOG_LEVEL is the only switch: the removed SKRUMI_LOG must not silence logs
SKRUMI_LOG=false SKRUMI_LOG_LEVEL=debug "$SKRUMI_BIN" "$LOG_SCRIPT" > "$OUT" 2>&1
grep -q '\[DEBUG\]' "$OUT" || { echo "FAIL: SKRUMI_LOG still overrides SKRUMI_LOG_LEVEL"; exit 1; }

echo "LOGGING_LEVELS_OK"
exit 0
