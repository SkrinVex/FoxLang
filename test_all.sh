#!/bin/bash

# Скрипт для запуска всех тестов FoxLang

echo "🦊 Running FoxLang Test Suite"
echo "=============================="

# Проверяем наличие интерпретатора
if [ ! -f "src/foxlang" ]; then
    echo "❌ Error: foxlang interpreter not found. Please build it first:"
    echo "   cd src && g++ main.cpp Lexer.cpp Parser.cpp -o foxlang"
    exit 1
fi

# Список тестов
tests=(
    "variables.fox"
    "functions.fox"
    "arrays.fox"
    "control_flow.fox"
    "math_operations.fox"
    "modules.fox"
    "builtin_functions.fox"
)

passed=0
failed=0

# Запускаем каждый тест
for test in "${tests[@]}"; do
    echo ""
    echo "🧪 Running test: $test"
    echo "----------------------------"
    
    if ./src/foxlang "test/$test"; then
        echo "✅ $test - PASSED"
        ((passed++))
    else
        echo "❌ $test - FAILED"
        ((failed++))
    fi
done

echo ""
echo "=============================="
echo "📊 Test Results:"
echo "   Passed: $passed"
echo "   Failed: $failed"
echo "   Total:  $((passed + failed))"

if [ $failed -eq 0 ]; then
    echo "🎉 All tests passed!"
    exit 0
else
    echo "💥 Some tests failed!"
    exit 1
fi
