#!/usr/bin/env bash
set -euo pipefail

FOXLANG_BIN="${1:-./foxlang}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_SCRIPT="$SCRIPT_DIR/logging_script.fox"
OUT="/tmp/fox_log_test.out"

# 1. FOXLANG_LOG_LEVEL=debug -> all messages
FOXLANG_LOG_LEVEL=debug "$FOXLANG_BIN" "$LOG_SCRIPT" > "$OUT" 2>&1
grep -q '\[DEBUG\] test-debug-msg' "$OUT" || { echo "FAIL: debug log missing"; exit 1; }
grep -q '\[INFO\] test-info-msg' "$OUT" || { echo "FAIL: info log missing in debug"; exit 1; }
grep -q '\[WARN\] test-warn-msg' "$OUT" || { echo "FAIL: warn log missing in debug"; exit 1; }
grep -q '\[ERROR\] test-error-msg' "$OUT" || { echo "FAIL: error log missing in debug"; exit 1; }

# 2. FOXLANG_LOG_LEVEL=info -> info, warn, error
FOXLANG_LOG_LEVEL=info "$FOXLANG_BIN" "$LOG_SCRIPT" > "$OUT" 2>&1
! grep -q '\[DEBUG\] test-debug-msg' "$OUT" || { echo "FAIL: debug log should be suppressed in info level"; exit 1; }
grep -q '\[INFO\] test-info-msg' "$OUT" || { echo "FAIL: info log missing"; exit 1; }
grep -q '\[WARN\] test-warn-msg' "$OUT" || { echo "FAIL: warn log missing"; exit 1; }
grep -q '\[ERROR\] test-error-msg' "$OUT" || { echo "FAIL: error log missing"; exit 1; }

# 3. FOXLANG_LOG_LEVEL=warn -> warn, error
FOXLANG_LOG_LEVEL=warn "$FOXLANG_BIN" "$LOG_SCRIPT" > "$OUT" 2>&1
! grep -q '\[DEBUG\] test-debug-msg' "$OUT" || { echo "FAIL: debug log should be suppressed in warn level"; exit 1; }
! grep -q '\[INFO\] test-info-msg' "$OUT" || { echo "FAIL: info log should be suppressed in warn level"; exit 1; }
grep -q '\[WARN\] test-warn-msg' "$OUT" || { echo "FAIL: warn log missing in warn level"; exit 1; }
grep -q '\[ERROR\] test-error-msg' "$OUT" || { echo "FAIL: error log missing in warn level"; exit 1; }

# 4. FOXLANG_LOG_LEVEL=error -> only error
FOXLANG_LOG_LEVEL=error "$FOXLANG_BIN" "$LOG_SCRIPT" > "$OUT" 2>&1
! grep -q '\[DEBUG\] test-debug-msg' "$OUT" || { echo "FAIL: debug log should be suppressed in error level"; exit 1; }
! grep -q '\[INFO\] test-info-msg' "$OUT" || { echo "FAIL: info log should be suppressed in error level"; exit 1; }
! grep -q '\[WARN\] test-warn-msg' "$OUT" || { echo "FAIL: warn log should be suppressed in error level"; exit 1; }
grep -q '\[ERROR\] test-error-msg' "$OUT" || { echo "FAIL: error log missing in error level"; exit 1; }

# 5. FOXLANG_LOG_LEVEL=off -> nothing
FOXLANG_LOG_LEVEL=off "$FOXLANG_BIN" "$LOG_SCRIPT" > "$OUT" 2>&1
! grep -q '\[DEBUG\]' "$OUT" || { echo "FAIL: log off leaked debug"; exit 1; }
! grep -q '\[INFO\]' "$OUT" || { echo "FAIL: log off leaked info"; exit 1; }
! grep -q '\[WARN\]' "$OUT" || { echo "FAIL: log off leaked warn"; exit 1; }
! grep -q '\[ERROR\]' "$OUT" || { echo "FAIL: log off leaked error"; exit 1; }

# 6. FOXLANG_LOG_LEVEL is the only switch: the removed FOXLANG_LOG must not silence logs
FOXLANG_LOG=false FOXLANG_LOG_LEVEL=debug "$FOXLANG_BIN" "$LOG_SCRIPT" > "$OUT" 2>&1
grep -q '\[DEBUG\]' "$OUT" || { echo "FAIL: FOXLANG_LOG still overrides FOXLANG_LOG_LEVEL"; exit 1; }

echo "LOGGING_LEVELS_OK"
exit 0
