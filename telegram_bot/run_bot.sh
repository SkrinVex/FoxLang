#!/bin/bash

# Скрипт запуска FoxLang Telegram Bot

echo "🦊 FoxLang Telegram Bot"
echo "======================="

# Проверяем наличие интерпретатора
if [ ! -f "../src/foxlang" ]; then
    echo "❌ Error: FoxLang interpreter not found!"
    echo "Please build it first:"
    echo "  cd ../src && g++ main.cpp Lexer.cpp Parser.cpp -o foxlang"
    exit 1
fi

# Проверяем наличие токена
if [ ! -f "token.txt" ] || [ ! -s "token.txt" ] || grep -q "YOUR_BOT_TOKEN_HERE" token.txt; then
    echo "❌ Error: Bot token not configured!"
    echo ""
    echo "Please configure your bot token:"
    echo "1. Get token from @BotFather in Telegram"
    echo "2. Edit token.txt file"
    echo "3. Replace YOUR_BOT_TOKEN_HERE with your actual token"
    exit 1
fi

echo "✅ Starting FoxLang Telegram Bot..."
echo ""

# Запускаем бота
../src/foxlang main.fox
