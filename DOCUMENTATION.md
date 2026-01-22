# 📚 Документация FoxLang v5.0

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
    return "FoxLang 5.0";
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

### Операторы сравнения

Операторы сравнения возвращают `1` (истина) или `0` (ложь).

| Оператор | Описание | Пример |
| --- | --- | --- |
| `==` | Равно | `x == 5` |
| `!=` | Не равно | `x != 5` |
| `<` | Меньше | `x < 10` |
| `>` | Больше | `x > 0` |

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

Массивы в FoxLang имеют фиксированный размер при создании.

1. **Создание:** `array имя размер;`
2. **Запись:** `set(имя, индекс, значение);`
3. **Чтение:** `get(имя, индекс)`
4. **Размер:** `size(имя)`

```cpp
array chest 3;     // Массив на 3 элемента
set(chest, 0, 55); // Записать 55 в индекс 0
print(get(chest, 0)); // Выведет 55
```

---

## 7. Модули и Импорт

FoxLang поддерживает импорт внешних модулей.
Используйте `include("путь/к/файлу.fox");`.

**Особенности:**

* При подключении внешних модулей код, находящийся вне функций, будет выполнен при импорте. Глобальные переменные и функции становятся доступными в текущем файле.

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
| `read_file(filename)` | Читает первую непустую строку из файла (игнорирует комментарии #). | `string config = read_file("config.txt");` |

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
| `json_get(json_string, key)` | Извлекает значение по ключу из JSON строки. Поддерживает ключи: "chat_id", "text", "update_id". | `string chat_id = json_get(response, "chat_id");` |
| `str_contains(text, substring)` | Проверяет, содержит ли строка подстроку. Возвращает `true` или `false`. | `bool found = str_contains("Hello World", "World");` |
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
    json_response("{\"message\":\"Welcome to FoxLang API!\",\"version\":\"5.0.1\"}");
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
# Компиляция
cd src && g++ -std=c++17 main.cpp Lexer.cpp Parser.cpp -o foxlang

# Запуск HTTP клиента
./foxlang examples/http_demo.fox

# Запуск веб-сервера
./foxlang examples/fastapi_demo.fox
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
- Сервер работает в фоновом режиме

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
