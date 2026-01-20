# 📚 Система библиотек FoxLang

## Как работает include

FoxLang поддерживает подключение внешних файлов через `include("filename.fox")`. Система работает следующим образом:

### Поиск файлов
1. **Относительный путь**: Сначала ищет файл относительно текущего .fox файла
2. **Абсолютный путь**: Если не найден, ищет рядом с исполняемым файлом

### Структура проекта
```
project/
├── main.fox           # Основной скрипт
├── libs/              # Папка с библиотеками
│   ├── net.fox        # Сетевые функции
│   ├── utils.fox      # Утилиты
│   └── math.fox       # Математические функции
└── foxlang            # Исполняемый файл
```

### Подключение библиотек
```cpp
// В main.fox
include("libs/net.fox");    // Относительно main.fox
include("libs/utils.fox");

// Теперь доступны функции из библиотек
string ip = get_public_ip();
int result = max(10, 20);
```

## 🌐 Библиотека net.fox

### Что изменилось
**Раньше**: net.fox был просто заглушкой с комментариями
**Сейчас**: Полноценная библиотека с удобными функциями высокого уровня

### Основные функции

#### HTTP клиент
```cpp
// Простой GET с обработкой ошибок
string response = get("https://api.example.com");

// POST с JSON данными
string result = post_json("https://api.example.com/data", "{\"key\":\"value\"}");

// Получение публичного IP
string ip = get_public_ip();
```

#### TCP клиент
```cpp
// Проверка доступности сервера
bool available = ping("google.com", 80);

// Отправка сообщения с автоматическим управлением соединением
string response = send_message("localhost", 8080, "Hello Server!");
```

#### Утилиты
```cpp
// Проверка интернет-соединения
if (is_online()) {
    print("Internet is available");
}
```

### Преимущества новой библиотеки

1. **Автоматическая обработка ошибок**: Не нужно проверять каждый вызов
2. **Упрощенный API**: Одна функция вместо нескольких низкоуровневых
3. **Готовые решения**: ping, проверка интернета, получение IP
4. **Безопасность**: Автоматическое закрытие соединений

## 🛠 Библиотека utils.fox

### Математические функции
```cpp
int maximum = max(15, 23);
int minimum = min(15, 23);
```

### Работа со строками
```cpp
string repeated = repeat("Fox", 3); // "FoxFoxFox"
bool starts = starts_with("FoxLang", "Fox"); // true
```

### Утилиты для массивов
```cpp
array numbers 5;
// ... заполнение массива ...

print_array(numbers, 5);    // Вывод содержимого
bubble_sort(numbers, 5);    // Сортировка
```

## 📝 Пример использования

```cpp
// main.fox
include("libs/net.fox");
include("libs/utils.fox");

print("=== FoxLang Project Demo ===");

// Сетевые функции
if (is_online()) {
    string ip = get_public_ip();
    print("Your IP: " + ip);
}

// Математика
int result = max(42, 17);
print("Maximum: " + result);

// Массивы
array data 3;
set(data, 0, 100);
set(data, 1, 50);
set(data, 2, 75);

print("Before sorting:");
print_array(data, 3);

bubble_sort(data, 3);

print("After sorting:");
print_array(data, 3);
```

## 🚀 Запуск

```bash
# Компиляция
cd src
g++ main.cpp Lexer.cpp Parser.cpp -o foxlang

# Запуск проекта с библиотеками
./foxlang ../test/example_with_libs.fox
```

## 💡 Создание собственных библиотек

1. Создайте файл в папке `libs/`
2. Определите функции
3. Подключите через `include("libs/your_lib.fox")`
4. Используйте функции в основном коде

Система автоматически найдет файл относительно вашего скрипта!
