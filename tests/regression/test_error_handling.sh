#!/usr/bin/env bash
set -euo pipefail

FOXLANG_BIN="${1:-./foxlang}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEMP_ERR_FOX="/tmp/fox_malformed.fox"

run_expect_fail() {
    local code="$1"
    local pattern="$2"
    local desc="$3"

    echo "$code" > "$TEMP_ERR_FOX"
    local out
    set +e
    out=$("$FOXLANG_BIN" "$TEMP_ERR_FOX" 2>&1)
    local status=$?
    set -e

    if [[ "$status" -eq 0 ]]; then
        echo "FAIL: Expected failure for $desc, but exited 0"
        exit 1
    fi

    if ! echo "$out" | grep -q "$pattern"; then
        echo "FAIL: Output for $desc did not contain '$pattern'. Output was: $out"
        exit 1
    fi
}

# 1. Missing semicolon / syntax error
run_expect_fail "int a = 5" "Syntax Error" "missing semicolon"

# 2. Unexpected token
run_expect_fail "int = 5;" "Syntax Error" "missing identifier in var decl"

# 3. Undefined variable
run_expect_fail "int a = undefined_var + 1;" "Variable 'undefined_var' not found" "undefined variable access"

# 4. Undefined function
run_expect_fail "non_existent_func(1, 2);" "Function 'non_existent_func' not found" "undefined function call"

# 5. Division by zero
run_expect_fail "int z = 10 / 0;" "Division by zero" "integer division by zero"

# 6. Array bounds
run_expect_fail "array arr 2; set(arr, 5, 10);" "Array index out of bounds" "array set out of bounds"
run_expect_fail "array arr 2; int x = get(arr, -1);" "Array index out of bounds" "array get negative index"

# 7. Type mismatch assignment
run_expect_fail "int a = 5; a = \"text\";" "Type Error" "assigning string to int variable"

# 8. Break outside loop in global scope
run_expect_fail "break;" "break" "break outside loop in global scope"

# 9. Non-existent file
set +e
out=$("$FOXLANG_BIN" "/path/to/definitely_not_existing_file_999.fox" 2>&1)
status=$?
set -e
if [[ "$status" -eq 0 ]]; then
    echo "FAIL: Expected non-zero status for missing file"
    exit 1
fi
if ! echo "$out" | grep -q "could not open file"; then
    echo "FAIL: Expected 'could not open file' in error output"
    exit 1
fi

rm -f "$TEMP_ERR_FOX"
echo "ERROR_HANDLING_OK"
exit 0
