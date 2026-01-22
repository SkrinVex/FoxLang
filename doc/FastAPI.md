# 🌐 FoxLang FastAPI - Веб-сервер библиотека

FoxLang теперь поддерживает создание веб-серверов в стиле FastAPI через библиотеку `net.fox`.

## 🚀 Быстрый старт

```cpp
include("src/net.fox");

void main() {
    // Запуск сервера на порту 8080
    start_server(8080);
    
    // Регистрация маршрутов
    register_get("/", "home_handler");
    register_post("/users", "create_user");
    
    print("Server running on http://localhost:8080");
}

// Обработчики
void home_handler() {
    json_response("{\"message\":\"Hello from FoxLang!\"}");
}

void create_user() {
    text_response("User created successfully");
}

main();
```

## 📋 API Функции

### Управление сервером

| Функция | Описание | Пример |
|---------|----------|---------|
| `start_server(port)` | Запускает HTTP сервер на указанном порту | `start_server(8080);` |
| `stop_server()` | Останавливает сервер | `stop_server();` |

### Регистрация маршрутов

| Функция | Описание | Пример |
|---------|----------|---------|
| `register_get(path, handler)` | Регистрирует GET маршрут | `register_get("/users", "get_users");` |
| `register_post(path, handler)` | Регистрирует POST маршрут | `register_post("/users", "create_user");` |

### Отправка ответов

| Функция | Описание | Пример |
|---------|----------|---------|
| `json_response(json)` | Отправляет JSON ответ | `json_response("{\"status\":\"ok\"}");` |
| `text_response(text)` | Отправляет текстовый ответ | `text_response("Hello World");` |

### HTTP клиент

| Функция | Описание | Пример |
|---------|----------|---------|
| `api_get(base_url, endpoint)` | GET запрос к API | `api_get("http://api.com", "/users");` |
| `api_post(base_url, endpoint, data)` | POST запрос к API | `api_post("http://api.com", "/users", data);` |
| `http_get_simple(url)` | Простой GET запрос | `http_get_simple("http://example.com");` |
| `post_json(url, json)` | POST с JSON данными | `post_json(url, "{\"key\":\"value\"}");` |

## 🎯 Примеры использования

### Простой API сервер

```cpp
include("src/net.fox");

void api_status() {
    json_response("{\"status\":\"healthy\",\"version\":\"1.0\"}");
}

void api_users() {
    json_response("{\"users\":[{\"id\":1,\"name\":\"Alice\"}]}");
}

void create_user() {
    text_response("User created with ID: 123");
}

void main() {
    start_server(3000);
    
    register_get("/status", "api_status");
    register_get("/users", "api_users");
    register_post("/users", "create_user");
    
    print("🚀 API Server running on http://localhost:3000");
    print("Endpoints:");
    print("  GET  /status - Health check");
    print("  GET  /users  - List users");
    print("  POST /users  - Create user");
}

main();
```

### HTTP клиент

```cpp
include("src/net.fox");

void test_external_api() {
    // Тест внешнего API
    string response = api_get("https://jsonplaceholder.typicode.com", "/posts/1");
    print("External API response: " + response);
    
    // Тест локального API
    string local = api_get("http://localhost:3000", "/status");
    print("Local API response: " + local);
}

test_external_api();
```

## 🛠 Встроенные функции

Следующие функции доступны на уровне интерпретатора:

- `server_start(port)` - Запуск HTTP сервера
- `server_stop()` - Остановка сервера  
- `route_get(path, handler)` - Регистрация GET маршрута
- `route_post(path, handler)` - Регистрация POST маршрута
- `send_response(data)` - Отправка ответа клиенту

## 🔧 Технические детали

- Сервер использует Python HTTP сервер под капотом
- Поддерживает JSON и текстовые ответы
- Автоматическая обработка CORS заголовков
- Простая маршрутизация по пути

## 📝 Примечания

- Сервер запускается в фоновом режиме
- Для остановки используйте `stop_server()` или Ctrl+C
- Поддерживаются только GET и POST методы
- JSON ответы автоматически получают правильный Content-Type
