# 🦊 FoxLang

![Version](https://img.shields.io/badge/version-5.0.1-orange) ![Language](https://img.shields.io/badge/language-C++17-blue) ![License](https://img.shields.io/badge/license-MIT-green)

**FoxLang** — современный интерпретируемый язык программирования общего назначения с поддержкой пользовательских функций, модульной системы и строгой типизацией.

## ✨ Ключевые возможности

- **🔧 Пользовательские функции:** Полная поддержка функций с параметрами и возвратом значений
- **📁 Модульная система:** Подключение библиотек через `include("lib.fox")` с поддержкой пользовательских функций
- **🔤 Современный синтаксис:** Идентификаторы с подчеркиваниями (`user_name`, `get_data`)
- **📦 Массивы:** Встроенная поддержка создания, чтения и записи массивов
- **🔄 Управление потоком:** Циклы `while`, `for` и условия `if/else`
- **🔢 Строгая типизация:** `int`, `float`, `string`, `bool`, `void` с автоматическим приведением
- **🧠 Логические операторы:** Поддержка `&&`, `||`, `!` для boolean логики
- **🛠 Безопасность:** Защита от крашей, информативные ошибки синтаксиса
- **🎲 Встроенные функции:** Математика, ввод/вывод, генерация чисел, чтение файлов

## 🚀 Быстрый старт

### 1. Сборка
```bash
cd src
g++ main.cpp Lexer.cpp Parser.cpp -o foxlang
```

### 2. Запуск
```bash
./foxlang script.fox
```

## 💻 Примеры кода

### Пользовательские функции
```cpp
int factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

void main() {
    int result = factorial(5);
    print("5! = " + result);
}

main();
```

### Работа с массивами
```cpp
void bubble_sort(array arr, int size) {
    int i = 0;
    while (i < size - 1) {
        int j = 0;
        while (j < size - i - 1) {
            if (get(arr, j) > get(arr, j + 1)) {
                int temp = get(arr, j);
                set(arr, j, get(arr, j + 1));
                set(arr, j + 1, temp);
            }
            j = j + 1;
        }
        i = i + 1;
    }
}
```

### Модульная система
```cpp
// math_lib.fox
int add(int a, int b) {
    return a + b;
}

float average(int a, int b) {
    return (a + b) / 2.0;
}

// main.fox
include("math_lib.fox");

int sum = add(10, 20);
float avg = average(15, 25);
print("Sum: " + sum + ", Average: " + avg);
```

### Чтение файлов
```cpp
// config.txt содержит: server_port=8080
string config = read_file("config.txt");
print("Config: " + config);

// Проверка успешного чтения
if (config != "") {
    print("✅ Config loaded successfully");
} else {
    print("❌ Failed to load config");
}
```

### HTTP запросы
```cpp
// Получение данных с API
string response = http_get("https://api.github.com/users/octocat");
if (response != "") {
    print("✅ API response received");
    print("Data: " + response);
} else {
    print("❌ Failed to fetch data");
}
```

## 📂 Структура проекта

* `src/` — Исходный код интерпретатора (C++)
* `test/` — Тесты функциональности
* `doc/` — Документация
* `CHANGELOG.md` — История изменений
* `DOCUMENTATION.md` — Полная документация языка

## 🤖 Telegram Bot

FoxLang поставляется с Telegram ботом, написанным на чистом FoxLang!

### Быстрый запуск бота:
```bash
# Настройте токен бота
cd telegram_bot
nano token.txt  # Вставьте токен от @BotFather

# Запустите бота
./run_bot.sh
```

Подробнее: [telegram_bot/README.md](telegram_bot/README.md)

## 🧪 Тестирование

```bash
# Запуск всех тестов
./test_all.sh

# Отдельные тесты
./src/foxlang test/variables.fox      # Тест переменных и типов
./src/foxlang test/functions.fox      # Тест пользовательских функций
./src/foxlang test/arrays.fox         # Тест массивов
./src/foxlang test/control_flow.fox   # Тест циклов и условий
./src/foxlang test/math_operations.fox # Тест математических операций
./src/foxlang test/modules.fox        # Тест модульной системы
./src/foxlang test/builtin_functions.fox # Тест встроенных функций
```

---

**Author:** [SkrinVex](https://skrinvex.su)  
**License:** MIT
