# 📚 Документация FoxLang v5.6.0

## Оглавление
1. [Основы синтаксиса](#1-основы-синтаксиса)
2. [Переменные и Типы](#2-переменные-и-типы)
3. [Пользовательские функции](#3-пользовательские-функции)
4. [Математика и Логика](#4-математика-и-логика)
5. [Управляющие конструкции](#5-управляющие-конструкции)
6. [Массивы](#6-массивы)
7. [Модули и Импорт](#7-модули-и-импорт)
8. [Встроенные функции](#8-встроенные-функции)
9. [Сетевые возможности и HTTP](#9-сетевые-возможности-и-http)
10. [Современный синтаксис](#10-современный-синтаксис)
11. [Standalone приложения](#21-standalone-приложения-foxlang-build)

---

## 1. Основы синтаксиса
FoxLang использует синтаксис, похожий на C++ и Java.
* Каждая команда **обязана** заканчиваться точкой с запятой `;`.
* Блоки кода выделяются фигурными скобками `{ ... }`.
* Комментарии начинаются с `//` и идут до конца строки.
* Поддерживаются идентификаторы с подчеркиваниями (`user_name`, `get_data`).

```cpp
// Это комментарий
print("Hello"); // Команда
string user_name = "john_doe"; // Современный синтаксис
```

---

## 2. Переменные и Типы

Язык поддерживает пять основных типов данных:

* `int` — Целые числа.
* `float` — Дробные числа с плавающей точкой.
* `string` — Текст в двойных кавычках.
* `bool` — Логический тип (`true` или `false`).
* `void` — Тип для функций без возвращаемого значения.

**Объявление:**

```cpp
int health = 100;
float gravity = 9.8;
string player_name = "Player1";
bool is_alive = true;
```

**Присваивание:**

```cpp
health = 90;
gravity = 1.62;
player_name = "Player2";
is_alive = false;
```

---

## 3. Пользовательские функции

FoxLang поддерживает полноценные пользовательские функции с параметрами и возвратом значений.

### Объявление функций

```cpp
// Функция с возвращаемым значением
int add(int a, int b) {
    return a + b;
}

// Функция без возвращаемого значения
void greet(string name) {
    print("Hello, " + name + "!");
}

// Функция без параметров
string get_version() {
    return "FoxLang 5.6.0";
}
```

### Вызов функций

```cpp
int result = add(5, 3);
greet("Alice");
string version = get_version();
print("Version: " + version);
```

### Рекурсивные функции

```cpp
int factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

int fact5 = factorial(5); // 120
```

### Локальные переменные

```cpp
int calculate_area(int width, int height) {
    int area = width * height; // Локальная переменная
    return area;
}
```

> **Важно:** Нельзя объявить переменную с именем, которое уже существует в текущей области видимости.
> При присваивании дробного значения переменной типа `int`, оно будет автоматически преобразовано в целое (отброшена дробная часть).

---

## 4. Математика и Логика

Поддерживаются стандартные арифметические операции с учетом приоритета.

### Арифметические операторы

| Оператор | Описание | Пример |
| --- | --- | --- |
| `+` | Сложение / Конкатенация строк | `5 + 5` или `"A" + "B"` |
| `-` | Вычитание | `10 - 2` |
| `*` | Умножение | `2 * 2` |
| `/` | Деление | `10 / 2` |
| `%` | Остаток от деления | `10 % 3` (вернет 1) |
| `++` | Инкремент (увеличение на 1) | `i++` (вернет старое значение, затем увеличит переменную) |
| `+=`, `-=`, `*=`, `/=` | Присваивание с операцией | `x += 5` (увеличит x на 5) |

### Операторы сравнения

Операторы сравнения возвращают `1` (истина) или `0` (ложь).

| Оператор | Описание | Пример |
| --- | --- | --- |
| `==` | Равно | `x == 5` |
| `!=` | Не равно | `x != 5` |
| `<` | Меньше | `x < 10` |
| `>` | Больше | `x > 0` |
| `<=` | Меньше или равно | `x <= 10` |
| `>=` | Больше или равно | `x >= 0` |

### Логические операторы

FoxLang поддерживает логические операторы для работы с boolean значениями:

| Оператор | Описание | Пример |
| --- | --- | --- |
| `&&` | Логическое И (AND) | `(x > 0) && (x < 10)` |
| `\|\|` | Логическое ИЛИ (OR) | `(x == 0) \|\| (x == 1)` |
| `!` | Логическое НЕ (NOT) | `!(x == 0)` |

```cpp
bool is_valid = (age >= 18) && (age <= 65);
bool is_weekend = (day == "Saturday") || (day == "Sunday");
bool is_not_empty = !name.empty();
```

---

## 5. Управляющие конструкции

### Условия (If / Else)

```cpp
int x = 10;
if (x == 10) {
    print("X is ten");
} else {
    print("X is not ten");
}
```

### Циклы (While)

Выполняет блок кода, пока условие истинно.

```cpp
int i = 0;
while (i < 5) {
    print("Loop iteration: " + i);
    i++;
}
```

### Циклы (For)

Классический цикл `for`, состоящий из инициализации, условия и шага.

```cpp
for (int i = 0; i < 5; i++) {
    print("For loop: " + i);
}
```

### Switch/Case конструкции

FoxLang поддерживает конструкции `switch/case` с поддержкой `break` и `default`:

```cpp
int day = 3;
switch (day) {
    case 1:
        print("Понедельник");
        break;
    case 2:
        print("Вторник");
        break;
    case 3:
        print("Среда");
        break;
    default:
        print("Другой день");
        break;
}
```

### Управление циклами

- `break` — Прерывает выполнение цикла или switch
- `continue` — Переходит к следующей итерации цикла

```cpp
for (int i = 0; i < 10; i++) {
    if (i == 5) {
        continue; // Пропустить 5
    }
    if (i == 8) {
        break; // Выйти из цикла
    }
    print(i);
}
```

### Области видимости (Scope)

Блоки кода `{ ... }` создают новую область видимости. Переменные, объявленные внутри блока, недоступны снаружи.

```cpp
int global = 10;
{
    int local = 5;
    print(global); // 10
    print(local);  // 5
}
// print(local); // Ошибка! Переменная local не существует здесь.
```

---

## 6. Массивы

Массивы в FoxLang поддерживают динамический размер и являются объектами первого класса (их можно передавать в функции).

1. **Создание:** `array имя размер;` (размер может быть переменной или выражением)
2. **Запись:** `set(имя, индекс, значение);`
3. **Чтение:** `get(имя, индекс)`
4. **Размер:** `size(имя)`

```cpp
int s = 3;
array chest s;     // Массив на 3 элемента
set(chest, 0, 55); // Записать 55 в индекс 0
print(get(chest, 0)); // Выведет 55
```

---

## 7. Модули и Импорт

FoxLang поддерживает импорт внешних модулей.
Используйте `include("путь/к/файлу.fox");`.

**Особенности:**

* `include` и `using` загружают объявления функций, переменных и вложенные импорты. Инициализаторы переменных выполняются при загрузке; обычные вызовы функций на верхнем уровне импортируемого файла пропускаются. Повторные и циклические импорты не загружают один модуль повторно.

---

## 8. Встроенные функции

### Ввод/Вывод
| Функция | Описание | Пример |
| --- | --- | --- |
| `print(expr...)` | Выводит текст или результат выражения в консоль. Может принимать несколько аргументов. | `print("Hello", name);` |
| `input()` | Ждет ввода строки от пользователя и возвращает её. | `string name = input();` |
| `input(prompt)` | Выводит приглашение и ждет ввода строки. | `string name = input("Имя: ");` |

### Математические функции
| Функция | Описание | Пример |
| --- | --- | --- |
| `round(number)` | Округляет дробное число до ближайшего целого. | `int x = round(3.7); // 4` |
| `random(min, max)` | Генерирует случайное число в диапазоне от min до max включительно. | `int dice = random(1, 6);` |

### Работа с файлами
| Функция | Описание | Пример |
| --- | --- | --- |
| `read_file(filename)` | Читает всё содержимое файла в строку (игнорируя пустые строки и комментарии `#`). | `string config = read_file("config.txt");` |
| `write_file(filename, content)` | Записывает строку в файл, перезаписывая его. Возвращает `true` при успехе. | `write_file("log.txt", "Started");` |
| `append_file(filename, content)` | Добавляет строку в конец файла. Возвращает `true` при успехе. | `append_file("log.txt", "Error!");` |

### HTTP запросы
| Функция | Описание | Пример |
| --- | --- | --- |
| `httpget(url)` | Выполняет HTTP GET запрос и возвращает ответ сервера. | `string data = httpget("https://api.example.com");` |
| `httppost(url, data)` | Выполняет HTTP POST запрос с данными. | `string response = httppost(url, "{\"key\":\"value\"}");` |
| `httppost(url, data, content_type)` | HTTP POST с указанием типа контента. | `httppost(url, data, "application/json");` |
| `httpput(url, data)` | Выполняет HTTP PUT запрос с данными. | `string response = httpput(url, data);` |
| `httpput(url, data, content_type)` | HTTP PUT с указанием типа контента. | `httpput(url, data, "text/plain");` |
| `httpdelete(url)` | Выполняет HTTP DELETE запрос. | `string response = httpdelete(url);` |

### FastAPI-подобный веб-сервер
| Функция | Описание | Пример |
| --- | --- | --- |
| `server_start(port)` | Запускает HTTP сервер на указанном порту. | `string result = server_start(8080);` |
| `server_stop()` | Останавливает HTTP сервер. | `string result = server_stop();` |
| `route_get(path, handler)` | Регистрирует GET маршрут с обработчиком. | `string result = route_get("/api", "handler");` |
| `route_post(path, handler)` | Регистрирует POST маршрут с обработчиком. | `string result = route_post("/users", "create");` |
| `send_response(data)` | Отправляет ответ клиенту (используется в обработчиках). | `send_response("{\"status\":\"ok\"}");` |

### Работа со строками и JSON
| Функция | Описание | Пример |
| --- | --- | --- |
| `json_get(json_string, path)` | Извлекает значение по вложенному пути (`message.chat.id`) и декодирует JSON escapes/Unicode `\\uXXXX`. | `string chat_id = json_get(update, "message.chat.id");` |
| `str_contains(text, substring)` | Проверяет, содержит ли строка подстроку. Возвращает `true` или `false`. | `bool found = str_contains("Hello World", "World");` |
| `str_replace(text, old, new)` | Заменяет все вхождения подстроки на новую строку. | `string res = str_replace("a b a", "a", "c");` |
| `str_split(text, delim)` | Разбивает строку по разделителю и возвращает массив. | `array words = str_split("a,b,c", ",");` |
| `size(string_or_array)` | Возвращает длину строки или размер массива. | `int len = size("Hello");` |
| `str_to_int(string)` | Преобразует строку в целое число. При ошибке возвращает 0. | `int num = str_to_int("123");` |

### Ввод с клавиатуры (низкоуровневый)
| Функция | Описание | Пример |
| --- | --- | --- |
| `getch()` | Читает один символ с клавиатуры без нажатия Enter. | `string key = getch();` |
| `kbhit()` | Проверяет, была ли нажата клавиша. Возвращает `true` или `false`. | `bool pressed = kbhit();` |

### Системные функции
| Функция | Описание | Пример |
| --- | --- | --- |
| `wait(milliseconds)` | Приостанавливает выполнение программы на указанное количество миллисекунд. | `wait(1000); // Пауза 1 секунда` |
| `fox()` | Пасхалка: выводит название языка "FoxLang". | `fox();` |

---

## 9. Сетевые возможности и HTTP

FoxLang предоставляет мощные возможности для работы с сетью и HTTP запросами. Поддерживаются все основные HTTP методы и создание веб-серверов.

### HTTP клиент - Отправка запросов

#### GET запросы
```cpp
// Простой GET запрос
string response = httpget("https://api.github.com/users/octocat");
print("Response: " + response);

// Получение JSON данных
string user_data = httpget("https://jsonplaceholder.typicode.com/users/1");
print("User: " + user_data);
```

#### POST запросы
```cpp
// POST с JSON данными
string json_data = "{\"name\":\"John\",\"email\":\"john@example.com\"}";
string response = httppost("https://jsonplaceholder.typicode.com/users", json_data, "application/json");
print("Created: " + response);

// POST без указания Content-Type (по умолчанию application/json)
string simple_post = httppost("https://httpbin.org/post", "{\"test\":\"data\"}");
print("POST result: " + simple_post);
```

#### PUT запросы (обновление данных)
```cpp
// Обновление существующего ресурса
string update_data = "{\"name\":\"John Updated\",\"email\":\"john.new@example.com\"}";
string updated = httpput("https://jsonplaceholder.typicode.com/users/1", update_data, "application/json");
print("Updated: " + updated);
```

#### DELETE запросы (удаление данных)
```cpp
// Удаление ресурса
string deleted = httpdelete("https://jsonplaceholder.typicode.com/users/1");
print("Deleted: " + deleted);
```

### Работа с различными API

#### Пример работы с REST API
```cpp
void work_with_api() {
    string base_url = "https://jsonplaceholder.typicode.com";
    
    // Получить список пользователей
    string users = httpget(base_url + "/users");
    print("All users: " + users);
    
    // Получить конкретного пользователя
    string user = httpget(base_url + "/users/1");
    print("User 1: " + user);
    
    // Создать новый пост
    string new_post = "{\"title\":\"My Post\",\"body\":\"Post content\",\"userId\":1}";
    string created = httppost(base_url + "/posts", new_post, "application/json");
    print("Created post: " + created);
    
    // Обновить пост
    string updated_post = "{\"id\":1,\"title\":\"Updated Post\",\"body\":\"New content\",\"userId\":1}";
    string updated = httpput(base_url + "/posts/1", updated_post, "application/json");
    print("Updated post: " + updated);
    
    // Удалить пост
    string deleted = httpdelete(base_url + "/posts/1");
    print("Deleted post: " + deleted);
}

work_with_api();
```

### FastAPI-подобный веб-сервер

FoxLang поддерживает создание веб-серверов через библиотеку `net.fox`:

#### Быстрый старт сервера
```cpp
include("src/net.fox");

// Обработчики маршрутов
void api_home() {
    json_response("{\"message\":\"Welcome to FoxLang API!\",\"version\":\"5.6.0\"}");
}

void api_users() {
    json_response("{\"users\":[{\"id\":1,\"name\":\"Alice\"},{\"id\":2,\"name\":\"Bob\"}]}");
}

void create_user() {
    json_response("{\"message\":\"User created\",\"id\":3,\"name\":\"Charlie\"}");
}

// Запуск сервера
void main() {
    // Старт сервера на порту 8080
    start_server(8080);
    
    // Регистрация маршрутов
    register_get("/", "api_home");
    register_get("/users", "api_users");
    register_post("/users", "create_user");
    
    print("🚀 Server running on http://localhost:8080");
    print("Available endpoints:");
    print("  GET  / - Welcome message");
    print("  GET  /users - List users");
    print("  POST /users - Create user");
}

main();
```

#### Встроенные серверные функции

| Функция | Описание | Пример |
|---------|----------|---------|
| `server_start(port)` | Запускает HTTP сервер | `server_start(8080);` |
| `server_stop()` | Останавливает сервер | `server_stop();` |
| `route_get(path, handler)` | Регистрирует GET маршрут | `route_get("/api", "handler");` |
| `route_post(path, handler)` | Регистрирует POST маршрут | `route_post("/users", "create");` |
| `send_response(data)` | Отправляет ответ клиенту | `send_response("{\"status\":\"ok\"}");` |

#### Библиотека net.fox - Высокоуровневые функции

```cpp
include("src/net.fox");

// Удобные функции из библиотеки:
start_server(8080);              // Запуск сервера
register_get("/", "handler");    // Регистрация GET маршрута
register_post("/api", "create"); // Регистрация POST маршрута
json_response("{\"key\":\"value\"}"); // JSON ответ
text_response("Hello World");    // Текстовый ответ
```

### Практические примеры

#### HTTP клиент для тестирования API
```cpp
void test_external_apis() {
    // Тест GitHub API
    string github_user = httpget("https://api.github.com/users/octocat");
    print("GitHub user: " + github_user);
    
    // Тест погодного API (пример)
    string weather = httpget("https://api.openweathermap.org/data/2.5/weather?q=Moscow&appid=YOUR_KEY");
    print("Weather: " + weather);
    
    // Отправка данных в webhook
    string webhook_data = "{\"text\":\"Hello from FoxLang!\"}";
    string webhook_response = httppost("https://hooks.slack.com/services/YOUR/WEBHOOK/URL", webhook_data);
    print("Webhook sent: " + webhook_response);
}
```

#### Простой API сервер с обработкой данных
```cpp
include("src/net.fox");

global array users 10;
global int user_count = 0;

void get_users() {
    string users_json = "{\"users\":[";
    int i = 0;
    while (i < user_count) {
        if (i > 0) {
            users_json = users_json + ",";
        }
        users_json = users_json + "{\"id\":" + i + ",\"name\":\"" + get(users, i) + "\"}";
        i = i + 1;
    }
    users_json = users_json + "],\"total\":" + user_count + "}";
    json_response(users_json);
}

void add_user() {
    if (user_count < 10) {
        set(users, user_count, "User" + user_count);
        user_count = user_count + 1;
        json_response("{\"message\":\"User added\",\"id\":" + (user_count - 1) + "}");
    } else {
        json_response("{\"error\":\"Maximum users reached\"}");
    }
}

void main() {
    start_server(3000);
    register_get("/users", "get_users");
    register_post("/users", "add_user");
    print("API server ready on http://localhost:3000");
}

main();
```

### Обработка ошибок и проверки

```cpp
void safe_http_request(string url) {
    string response = httpget(url);
    
    if (response == "") {
        print("❌ Request failed: " + url);
        return;
    }
    
    // Проверка на успешный ответ (простая проверка)
    if (str_contains(response, "error") || str_contains(response, "Error")) {
        print("⚠️ API returned error: " + response);
        return;
    }
    
    print("✅ Success: " + response);
}

// Использование
safe_http_request("https://api.github.com/users/nonexistent");
safe_http_request("https://api.github.com/users/octocat");
```

### Краткий справочник HTTP функций

#### 📋 Все встроенные HTTP функции
| Функция | Описание | Пример |
|---------|----------|---------|
| `httpget(url)` | GET запрос | `httpget("https://api.com/users")` |
| `httppost(url, data)` | POST запрос | `httppost(url, "{\"key\":\"value\"}")` |
| `httppost(url, data, type)` | POST с Content-Type | `httppost(url, data, "application/json")` |
| `httpput(url, data)` | PUT запрос | `httpput(url, "{\"updated\":true}")` |
| `httpput(url, data, type)` | PUT с Content-Type | `httpput(url, data, "text/plain")` |
| `httpdelete(url)` | DELETE запрос | `httpdelete("https://api.com/item/1")` |

#### 🚀 Серверные функции (встроенные)
| Функция | Описание | Пример |
|---------|----------|---------|
| `server_start(port)` | Запуск сервера | `server_start(8080)` |
| `server_stop()` | Остановка сервера | `server_stop()` |
| `route_get(path, handler)` | GET маршрут | `route_get("/api", "handler")` |
| `route_post(path, handler)` | POST маршрут | `route_post("/users", "create")` |
| `send_response(data)` | Отправка ответа | `send_response("{\"ok\":true}")` |

#### 📚 Библиотека net.fox (высокоуровневые функции)
| Функция | Описание | Пример |
|---------|----------|---------|
| `start_server(port)` | Удобный запуск сервера | `start_server(8080)` |
| `register_get(path, handler)` | Регистрация GET | `register_get("/", "home")` |
| `register_post(path, handler)` | Регистрация POST | `register_post("/api", "create")` |
| `json_response(json)` | JSON ответ | `json_response("{\"status\":\"ok\"}")` |
| `text_response(text)` | Текстовый ответ | `text_response("Hello World")` |

#### ⚡ Быстрые примеры

**HTTP клиент:**
```cpp
// GET
string user = httpget("https://api.github.com/users/octocat");

// POST
string data = "{\"name\":\"John\"}";
string created = httppost("https://api.com/users", data, "application/json");

// PUT
string updated = httpput("https://api.com/users/1", "{\"name\":\"Jane\"}");

// DELETE
string deleted = httpdelete("https://api.com/users/1");
```

**Веб-сервер:**
```cpp
include("src/net.fox");

void api_home() {
    json_response("{\"message\":\"Hello FoxLang API!\"}");
}

void main() {
    start_server(8080);
    register_get("/", "api_home");
    print("🚀 Server: http://localhost:8080");
}

main();
```

#### 🧪 Компиляция и запуск
```bash
# Сборка runtime через CMake
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Запуск HTTP клиента
./build/foxlang examples/http_demo.fox

# Запуск веб-сервера
./build/foxlang examples/fastapi_demo.fox
```

### Расширенные примеры использования

#### Webhook отправка
```cpp
void send_notification(string message) {
    string webhook_url = "https://hooks.slack.com/services/YOUR/WEBHOOK/URL";
    string payload = "{\"text\":\"" + message + "\"}";
    string response = httppost(webhook_url, payload);
    
    if (response != "") {
        print("✅ Notification sent");
    } else {
        print("❌ Failed to send notification");
    }
}

send_notification("Hello from FoxLang!");
```

#### Полный REST API сервер с данными
```cpp
include("src/net.fox");

global array users 10;
global int user_count = 0;

void get_users() {
    string users_json = "{\"users\":[";
    int i = 0;
    while (i < user_count) {
        if (i > 0) {
            users_json = users_json + ",";
        }
        users_json = users_json + "{\"id\":" + i + ",\"name\":\"" + get(users, i) + "\"}";
        i = i + 1;
    }
    users_json = users_json + "],\"total\":" + user_count + "}";
    json_response(users_json);
}

void add_user() {
    if (user_count < 10) {
        set(users, user_count, "User" + user_count);
        user_count = user_count + 1;
        json_response("{\"message\":\"User added\",\"id\":" + (user_count - 1) + "}");
    } else {
        json_response("{\"error\":\"Maximum users reached\"}");
    }
}

void main() {
    start_server(3000);
    register_get("/users", "get_users");
    register_post("/users", "add_user");
    print("API server ready on http://localhost:3000");
}

main();
```

#### Тестирование множественных API
```cpp
void test_multiple_apis() {
    print("🧪 Testing multiple APIs:");
    
    // GitHub API
    string github = httpget("https://api.github.com/users/octocat");
    print("GitHub API: " + github);
    
    // JSONPlaceholder API
    string posts = httpget("https://jsonplaceholder.typicode.com/posts/1");
    print("JSONPlaceholder: " + posts);
    
    // HTTPBin для тестирования POST
    string test_post = httppost("https://httpbin.org/post", "{\"test\":\"data\"}");
    print("HTTPBin POST: " + test_post);
    
    // Создание нового поста
    string new_post = "{\"title\":\"FoxLang Test\",\"body\":\"API testing\"}";
    string created = httppost("https://jsonplaceholder.typicode.com/posts", new_post);
    print("Created post: " + created);
}

test_multiple_apis();
```

#### Простой счетчик API
```cpp
include("src/net.fox");

global int counter = 0;

void get_counter() {
    json_response("{\"counter\":" + counter + ",\"message\":\"Current value\"}");
}

void increment_counter() {
    counter = counter + 1;
    json_response("{\"counter\":" + counter + ",\"message\":\"Incremented\"}");
}

void decrement_counter() {
    counter = counter - 1;
    json_response("{\"counter\":" + counter + ",\"message\":\"Decremented\"}");
}

void reset_counter() {
    counter = 0;
    json_response("{\"counter\":0,\"message\":\"Reset to zero\"}");
}

void main() {
    start_server(4000);
    
    register_get("/counter", "get_counter");
    register_post("/counter/increment", "increment_counter");
    register_post("/counter/decrement", "decrement_counter");
    register_post("/counter/reset", "reset_counter");
    
    print("🔢 Counter API running on http://localhost:4000");
    print("Available endpoints:");
    print("  GET  /counter - Get current value");
    print("  POST /counter/increment - Add 1");
    print("  POST /counter/decrement - Subtract 1");
    print("  POST /counter/reset - Reset to 0");
}

main();
```

### Технические особенности

#### Content-Type заголовки
- По умолчанию POST/PUT используют `application/json`
- Можно указать свой: `httppost(url, data, "text/plain")`
- Поддерживаются: `application/json`, `text/plain`, `application/x-www-form-urlencoded`

#### Обработка ответов
- Все функции возвращают `string` с телом ответа
- Пустая строка `""` означает ошибку соединения
- HTTP коды ошибок (404, 500) возвращают тело ответа сервера

#### Сервер
- Использует простую реализацию HTTP сервера
- Поддерживает GET и POST методы
- JSON ответы автоматически получают правильный Content-Type
- `listen` / `server_start` блокирует выполнение до остановки сервера; сетевой runtime реализован для Linux и Windows

---

## 10. Современный синтаксис

FoxLang поддерживает современные соглашения по именованию и синтаксису:

### Идентификаторы с подчеркиваниями

В отличие от старых версий, FoxLang теперь полностью поддерживает идентификаторы с подчеркиваниями:

```cpp
// Переменные
string user_name = "john_doe";
int max_health = 100;
bool is_game_over = false;

// Функции
void calculate_damage(int base_damage, float multiplier) {
    // ...
}

int get_player_score() {
    return player_score;
}
```

### Глобальные переменные

FoxLang поддерживает объявление глобальных переменных с ключевым словом `global`:

```cpp
global int game_score = 0;
global string player_name = "Unknown";

void update_score(int points) {
    game_score = game_score + points;
}

void main() {
    print("Score: " + game_score);
    update_score(100);
    print("New score: " + game_score);
}
```

### Примеры современного кода

```cpp
// Современная функция с подчеркиваниями
bool check_user_permissions(string user_role, int required_level) {
    if (user_role == "admin") {
        return true;
    }
    
    int user_level = get_user_level(user_role);
    return user_level >= required_level;
}

// Работа с массивами
void sort_user_scores(array scores, int count) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (get(scores, j) > get(scores, j + 1)) {
                int temp = get(scores, j);
                set(scores, j, get(scores, j + 1));
                set(scores, j + 1, temp);
            }
        }
    }
}
```

---


---

## 11. Стандартная библиотека FoxLang 5.2

### Поиск и подключение модулей

`include("path.fox")` по-прежнему поддерживается. Начиная с FoxLang 5.2 конструкция `using module;` используется для подключения модулей стандартной библиотеки.

Порядок поиска:

Сначала проверяется `std/module.fox`, затем `module.fox`. Каждый вариант ищется
относительно импортирующего файла, текущего каталога, `FOXLANG_HOME` и
`FOXLANG_HOME/std`. Если файлов нет, используется встроенная стандартная библиотека.
Ошибка выполнения найденного модуля не приводит к поиску другого файла.

Уже загруженный модуль повторно не выполняется в рамках одного процесса интерпретатора.

### Стандартные модули

| Модуль | Назначение |
|---|---|
| `terminal` | очистка терминала, перемещение курсора, ANSI-цвета и вывод без переноса строки |
| `net` | DNS и TCP-клиентские соединения на Linux и Windows |
| `http` | удобные обёртки для GET/POST/PUT/DELETE |
| `math` | `clamp`, `min`, `max` и математические помощники |
| `string` | поиск, замена и преобразование строк |
| `time` | задержки и время в миллисекундах |

### TCP API

```cpp
using net;

string ip = resolve_host("example.com");
int sock = connect_tcp("example.com", 80);
int sent = send_tcp(sock, "GET / HTTP/1.0\\r\\nHost: example.com\\r\\n\\r\\n");
string chunk = recv_tcp(sock, 4096);
bool closed = close_tcp(sock);
```

Значение сокета меньше нуля означает ошибку подключения. `recv_tcp` возвращает пустую строку, если соединение закрыто удалённой стороной или произошла ошибка чтения.

### Терминальный API

```cpp
using terminal;

clear();
hide_cursor();
goto_xy(2, 4);
color(36);
write("FoxLang");
reset_color();
show_cursor();
```

### HTTP/webhook-сервер

FoxLang содержит HTTP-сервер для Linux и Windows. Маршруты регистрируются до вызова `listen()`.

```cpp
using server;

void webhook() {
    string payload = body();
    respond_status(200, "{\"ok\":true}");
}

void main() {
    post("/webhook", "webhook");
    listen(8080);
}

main();
```

Доступны `get`, `post`, `body`, `method`, `path`, `respond`, `respond_status` и `listen`. Сервер принимает реальные TCP/HTTP-запросы и передаёт тело запроса обработчику.

## 11. Конфигурация, .env и секреты

Перед запуском скрипта FoxLang автоматически загружает `.env` рядом со скриптом, а затем при необходимости `.env` текущего каталога. Уже заданные системные переменные имеют приоритет.

```dotenv
TELEGRAM_BOT_TOKEN=replace_me
FOXLANG_LOG_LEVEL=info
```

`using env;` предоставляет:
- `env("NAME")` — возвращает значение переменной окружения или пустую строку, если она не задана;
- `secret("NAME")` — возвращает значение обязательного секрета; если переменная отсутствует или пуста, программа немедленно завершается с понятной ошибкой (Runtime Error);
- `env_default("NAME", "fallback")` — возвращает значение переменной окружения `NAME`, а если она отсутствует или пуста, возвращает значение по умолчанию `fallback`.

Файлы `.env` исключены из Git; `.env.example` можно хранить как публичный шаблон.

---

## 12. Логирование

`using log;` предоставляет функции:
- `debug(message)`
- `info(message)`
- `warn(message)`
- `error(message)`

Сообщения выводятся в `stderr`.

Переменная `FOXLANG_LOG_LEVEL` задаёт порог отображения: `debug`, `info`, `warn`, `error`, `off`. По умолчанию активен уровень `info`.

Для обратной совместимости флаг `FOXLANG_LOG=false` (а также `0`, `off`, `no`) полностью отключает вывод логов.

---

## 13. HTTP/webhook-сервер

На Linux и Windows FoxLang предоставляет встроенный HTTP runtime:
- `get(path, handler_func_name)` — регистрация GET-обработчика;
- `post(path, handler_func_name)` — регистрация POST-обработчика;
- `body()` — получение тела запроса;
- `method()` — метод HTTP запроса (GET/POST);
- `path()` — путь запроса;
- `respond(data)` — отправка ответа с кодом 200;
- `respond_status(status, data)` — отправка ответа с произвольным HTTP-кодом (например, 201 или 400);
- `listen(port)` — запуск цикла обработки входящих запросов;
- `server_stop()` — корректная остановка сервера после обработки текущего запроса.

```cpp
using server;

void webhook() {
    string payload = body();
    respond_status(200, "{\"ok\":true}");
    // server_stop(); // для остановки сервера из обработчика
}

void main() {
    get("/health", "webhook");
    post("/telegram", "webhook");
    listen(8080);
}

main();
```

`listen()` слушает HTTP; `listen_tls()` включает встроенный HTTPS. Reverse proxy можно
использовать по выбору, но он не требуется для TLS.

### HTTPS-сервер и доверенные CA

```cpp
using server;
using env;
void health() { respond("ready"); }
get("/health", "health");
listen_tls(8443, secret("TLS_CERT_FILE"), secret("TLS_KEY_FILE"));
```

`listen_tls(int port, string certificate, string private_key)` использует TLS 1.2
или новее через статический Mbed TLS на Linux и Windows. Аргументы — пути к
PEM-файлам во время запуска: сертификат (leaf первым, затем intermediate chain) и
его незашифрованный приватный ключ. Отсутствующие, повреждённые или несовпадающие
credentials завершают программу с ошибкой до открытия порта. Ошибка handshake
закрывает только соединение; сервер продолжает работать и не переключается на HTTP.
Маршруты, `body`, `respond`, JSON и `server_stop` используются как в обычном сервере.
Сервер обслуживает соединения последовательно, с ограничением I/O соединения
10 сек. и отдельного блокирующего чтения/записи 5 сек. mTLS, автоматическое получение
и продление сертификата и горячая смена ключа пока не реализованы; для обновления
сертификата перезапустите сервер. Приватные ключи не должны находиться в исходниках:
передавайте пути или подключайте хранилище секретов во время запуска.

Для исходящих HTTPS-запросов по умолчанию встроен публичный CA snapshot Mozilla
от 2026-08-13. У получателя не требуется отдельный пакет CA. Проверки цепочки и
имени сервера обязательны. При необходимости задайте `FOXLANG_CA_BUNDLE`:

- путь к PEM-файлу — заменить набор доверенных CA;
- `embedded` — явно использовать встроенный snapshot;
- `system` — явно использовать доверенное хранилище ОС.

На Linux `SSL_CERT_FILE` учитывается, когда `FOXLANG_CA_BUNDLE` не задан.
Ошибочный путь вызывает ошибку проверки TLS, а не отключение проверки.
Пользовательские CA, сертификат сервера и его ключ автоматически не встраиваются.
Для обновления встроенного snapshot обновите FoxLang и пересоберите standalone;
происхождение, лицензия и порядок обновления описаны в [resources/ca](resources/ca/README.md).

---

## 14. Работа с JSON

Модуль `using json;` предоставляет:
- `json_path(json_string, path)` — извлечение значения по вложенному точечному пути (например, `message.chat.id`, `message.from.first_name`, `user.name`);
- `json_safe(text)` — безопасное экранирование специальных символов (`"`, `\`, переносов строк, табуляций) для формирования корректного JSON-документа.

Парсер поддерживает:
- строки, целые и дробные числа, булевы значения;
- стандартные управляющие символы (`\n`, `\t`, `\r`, `\"`, `\\`);
- последовательности Unicode `\uXXXX` (включая русские/кириллические символы);
- суррогатные пары UTF-16 для emoji (например, `\uD83E\uDD8A` -> 🦊).

---

## 15. CLI и ссылки

```bash
foxlang program.fox
foxlang build program.fox -o program
foxlang build program.fox --output program
foxlang --version
foxlang --help
```

Репозиторий: https://github.com/SkrinVex/FoxLang  
Документация: https://github.com/SkrinVex/FoxLang/blob/master/DOCUMENTATION.md

---

## 16. Архитектура ядра и C++ API библиотеки (foxlang_core)

Реализация FoxLang отделена от консольного интерфейса и скомпонована в статическую библиотеку `foxlang_core`. Консольная команда `foxlang` — это тонкая обёртка над C++ API.

### Подключение в C++:

```cpp
#include <foxlang/FoxLang.h>
#include <iostream>

int main() {
    foxlang::Interpreter interpreter;

    // Выполнение кода из строки
    foxlang::RunResult res = interpreter.runSource("int x = 20; int y = 22; int z = x + y;");
    if (res.success) {
        std::cout << "Результат: " << interpreter.getGlobal("z").value << std::endl;
    } else {
        std::cerr << "Ошибка: " << res.errorMessage << std::endl;
    }

    // Выполнение файла
    foxlang::RunResult fileRes = interpreter.runFile("script.fox");
    return fileRes.exitCode;
}
```

### Основные классы C++ API:
- `foxlang::Interpreter` — основной экземпляр рантайма FoxLang. Хранит глобальный контекст переменных, функций и кеш модулей;
- `foxlang::InterpreterOptions` — настройки интерпретатора (`foxHome`, `loadDotEnv`, `workingDir`, `sources`);
- `foxlang::RunResult` — результат выполнения программы: `bool success`, `int exitCode`, `std::string errorMessage`;
- `foxlang::Lexer` — токенизатор исходного кода, формирующий список `Token` с номерами строк и колонок;
- `foxlang::Parser` — синтаксический анализатор, формирующий дерево AST (`BlockNode`) без немедленного выполнения;
- `foxlang::Context` — иерархическая таблица символов (переменные, массивы, функции);
- `foxlang::platform` — абстракция системных вызовов (сокеты, терминал, процессы).

---

## 17. Сборка, тестирование и разработка (CMake/CTest)

Стандартная сборка требует CMake 3.18+, компиляторы C и C++17. При первой
конфигурации загружаются libcurl 8.22.0 и Mbed TLS 3.6.7 с проверкой
SHA-256. Для offline-сборки укажите распакованные исходники через
`FETCHCONTENT_SOURCE_DIR_CURL` и `FETCHCONTENT_SOURCE_DIR_MBEDTLS`.
Тестам нужны Python 3 и `openssl`; Linux shell-тесту также нужен `curl`.
Эти утилиты не требуются готовым программам.

```bash
# Конфигурация и сборка
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Запуск набора регрессионных и unit-тестов
ctest --test-dir build -C Release --output-on-failure
```

Все тесты завершаются с ненулевым кодом при обнаружении расхождений.

---

## 18. Языковой сервер foxlang-lsp и интеграция с редакторами

В состав FoxLang входит полнофункциональный языковой сервер `foxlang-lsp` (LSP 3.17) и готовые плагины для **VS Code** и **Kate**:

```text
    FoxLang исходный код
             ↓
           Lexer (UTF-8 байты и UTF-16 code units)
             ↓
           Parser (SourceRange для AST + восстановление после синтаксических ошибок)
             ↓
            AST
          ↙     ↘
  Semantic Analyzer   Runtime (eval)
         ↓
     foxlang-lsp (JSON-RPC stdio)
     ↙         ↘
VS Code        Kate
```

1. **Разделение анализа и исполнения**: `SemanticAnalyzer` выполняет статический анализ без вызова `eval()`, что гарантирует безопасность (пользовательский код не исполняется во время редактирования).
2. **Точные диапазоны и UTF-16**: отслеживаются диапазоны `SourceRange` для всех объявлений и выражений, с корректным подсчётом смещений в UTF-16 для поддержки кириллицы и эмодзи (`🦊`).
3. **Диагностика**: регистрация синтаксических ошибок парсера и семантических ошибок (необъявленные переменные/функции, дубликаты, несоответствие числа аргументов, возврат значения из `void` функции).
4. **Возможности LSP**:
   - `textDocument/hover`: сигнатуры функций и типы переменных;
   - `textDocument/completion`: автодополнение ключевых слов, функций stdlib и пользовательских символов;
   - `textDocument/definition`: переход к месту объявления (`F12`);
   - `textDocument/documentSymbol`: навигация по структуре файла.

Подробные инструкции по настройке VS Code и Kate см. в [docs/EDITORS.md](docs/EDITORS.md).

---

## 19. Подготовка к встраиванию в Android (JNI)

Ядро FoxLang может компилироваться с помощью Android NDK в разделяемую библиотеку `libfoxlang.so` и вызываться из Kotlin/Java через JNI.

### Особенности платформы Android:
1. **HTTPS**: HTTP-клиент использует статический libcurl, без внешней команды `curl`. Для Android ещё нужно проверить сборку TLS backend и доступ к доверенным сертификатам; эта платформа пока не подтверждена тестами.
2. **Терминал**: интерактивные вызовы `getch()` и `kbhit()` требуют наличия TTY в `stdin` и не применяются в контексте Android GUI.
3. **Разрешения сети**: использование сетевых сокетов и HTTP-сервера требует объявления `<uses-permission android:name="android.permission.INTERNET" />`.
4. **Стандартная библиотека**: пути к модулям (`std/`) на Android должны конфигурироваться через `options.foxHome` во внутреннее хранилище приложения (`context.filesDir`).

---

## 20. Запуск в Docker и публикация контейнеров

FoxLang поддерживает исполнение в легковесных изолированных контейнерах Docker на базе Alpine Linux.

Официальный контейнерный образ с интерпретатором `foxlang`, сервером `foxlang-lsp` и стандартной библиотекой:
```bash
ghcr.io/skrinvex/foxlang:latest
```

### Запуск файла скрипта:
```bash
docker run --rm -v $(pwd):/app ghcr.io/skrinvex/foxlang:latest script.fox
```

### Развёртывание бота / HTTP-сервера:
```bash
docker run -d --name foxbot -p 8080:8080 \
  -e TELEGRAM_BOT_TOKEN="my_secret_token" \
  -v $(pwd):/app \
  ghcr.io/skrinvex/foxlang:latest bot.fox
```

### Автоматическая сборка в CI:
Каждый push в ветку `master` и теги версий `v*` автоматически собирают и публикуют Docker-образ в GitHub Container Registry (`ghcr.io/skrinvex/foxlang`) через рабочий процесс `.github/workflows/docker.yml`.




## 21. Standalone приложения: `foxlang build`

```bash
foxlang app.fox                      # Запуск исходника через установленный FoxLang
foxlang build app.fox -o app          # Упаковка для распространения
foxlang build app.fox --output app    # Та же команда
```

Linux: `./app`. Windows: `app.exe` (в PowerShell — `.\app.exe`). Без `-o`
выходной файл получает имя исходника без расширения и создаётся в текущем каталоге;
на Windows добавляется `.exe`. Путь с пробелами заключайте в кавычки.
Существующий файл не перезаписывается. Ошибки аргументов, чтения, синтаксиса,
отсутствующие модули и повреждённый bundle дают ненулевой exit code.

**Получателю программы не требуется устанавливать FoxLang.** Один executable
содержит общее ядро `foxlang_core`, исходник и разрешённые зависимости.
Для упаковки и запуска не нужны CMake, C++ compiler или SDK. CMake и compiler
нужны только для первоначальной сборки самого FoxLang из репозитория.
Linux FoxLang создаёт Linux executable, Windows FoxLang — Windows executable.
Cross-build командой `foxlang build` не поддерживается.

Это standalone packaging / runtime bundling, а не компиляция FoxLang в native
machine code. Исходники хранятся внутри файла и могут быть извлечены: упаковка
не является защитой кода или секретов.

### Модули

Вся небольшая официальная stdlib встроена в CLI на этапе сборки CMake.
`foxlang build` использует обычные Lexer/Parser и резолвер модулей, рекурсивно
собирает `using`/`include` и сохраняет граф разрешённых импортов. Пользовательский
код при этом не выполняется. Повторные импорты и циклы не дублируют исходники.
При запуске standalone модули читаются из памяти без обращения к `FOXLANG_HOME`,
исходной stdlib или каталогу разработчика.

```text
project/
  main.fox       # include("utils.fox"); print(greeting());
  utils.fox      # string greeting() { return "Hello"; }
```

Импорты собираются также из функций и неисполненных веток. Все такие зависимости
должны существовать при сборке. Вычисляемые имена `include` не поддерживаются
существующим синтаксисом языка. Сборщик не ищет зависимости по текстовым маркерам
или регулярным выражениям.

### Ресурсы, окружение и секреты

`read_file("config.json")` читает внешний файл относительно рабочего каталога
процесса. `config.json` и остальные ресурсы автоматически не упаковываются.
Запись файлов и терминальные функции сохраняют обычное поведение runtime.

**`.env` и переменные окружения времени сборки не встраиваются.** Standalone
получает переменные процесса во время запуска; `env`, `secret`, `env_default`
и уровни логирования продолжают работать. Автозагрузка `.env` для standalone
отключена. Обычный `foxlang app.fox` сохраняет прежнюю автозагрузку `.env`.
Секреты, записанные непосредственно в исходниках, окажутся внутри executable.

### Системные зависимости и совместимость

- HTTP(S)-клиент использует статический libcurl без shell, `popen` и отдельного
  `curl` в PATH. Linux включает Mbed TLS статически; Windows использует Schannel.
  Проверки цепочки сертификатов и имени сервера включены всегда. Публичные CA
  Mozilla встроены. `FOXLANG_CA_BUNDLE` выбирает PEM-файл, `embedded` или `system`;
  на Linux также учитывается `SSL_CERT_FILE`. Пользовательские CA не упаковываются.
- HTTP-сервер, DNS и TCP реализованы через POSIX sockets на Linux и Winsock на
  Windows. Сервер обрабатывает соединения последовательно: HTTP/1.0–1.1,
  `Content-Length`, до 64 KiB заголовков и 1 MiB тела; chunked-запросы не поддержаны.
  `listen_tls` обеспечивает встроенный HTTPS без reverse proxy. Исключение обработчика даёт
  HTTP 500. Клиент ограничивает ответ 16 MiB, подключение — 10 сек., запрос — 35 сек.
- Лицензии сетевых библиотек доступны через `--foxlang-licenses` у CLI и у любого
  standalone executable. Этот аргумент зарезервирован и не запускает программу.
- Windows-сборка MSVC использует статический CRT (`/MT`), MinGW — статические
  библиотеки компилятора. Отдельные DLL FoxLang или Visual C++ Redistributable
  не нужны; системные DLL Windows остаются необходимы.
- Linux-пакет и Docker target `portable` включают musl/libm, C++ и сетевые
  библиотеки статически, без отдельного ELF-загрузчика и shared libraries.
  Проверяются сборка в Alpine и запуск на Ubuntu. Нужны Linux x86_64, `/proc`
  и поддерживаемые системные вызовы; это не бинарник для любой ОС/архитектуры.
  Обычная локальная CMake-сборка Linux/GCC включает libstdc++/libgcc, но сохраняет
  системные libc/libm. Для переносимого варианта используйте Docker target `portable`
  либо musl toolchain с `-DFOXLANG_STATIC_LINUX=ON`.
- Форматы первой реализации: Linux ELF64 x86_64 и Windows PE32+ x86_64.
  Не применяйте `strip`, UPX или подпись к уже упакованному executable:
  это изменяет контролируемые смещения/длину. Подписанные PE-stub не принимаются.

Подробная спецификация формата, ограничения и проверки:
[docs/STANDALONE.md](docs/STANDALONE.md). Настоящий AOT backend в будущем потребует
семантического анализа и типизации, IR, генерации native-кода, ABI runtime и
отдельного набора проверок эквивалентности интерпретатору.
