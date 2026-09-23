# FoxLang 5.5.2 — Changelog

- **Улучшенная подсветка синтаксиса**:
  - VS Code: обновлена TextMate-грамматика — поддержаны многострочные комментарии `/* ... */`, теги `TODO:` / `@param`, подсветка директив модулей `using`/`include`, объявления и параметры функций, шестнадцатеричные числа, строковые escape-последовательности `\uXXXX` и полный список функций стандартной библиотеки (`secret`, `listen`, `respond`, `info`, `warn`, `error` и др.).
  - Kate: обновлена XML-схема KSyntaxHighlighting — добавлены блочные комментарии, сворачивание кода (folding блоков `{}`), Unicode-экранирование и встроенные функции.
- **Автономный LSP-клиент для VS Code (Zero-Dependency)**:
  - Клиент расширения переписан на чистый VS Code Extension Host API и встроенный Node.js без внешних npm-зависимостей (`vscode-languageclient`). Расширение работает сразу после установки без `npm install`.
- **Надёжная установка расширения VS Code**:
  - Исправлено имя каталога расширения на `<publisher>.<name>-<version>` (`foxlang.foxlang-5.5.2`) в `install.sh`.
  - Добавлена автоматическая сборка и установка пакетов `.vsix` (`code --install-extension`).
  - Добавлена очистка записей `.obsolete` при переустановке.
  - В артефакты релиза включён официальный готовый файл `foxlang-5.5.2.vsix`.

---

# FoxLang 5.5.1 — Changelog

- **Автоматическая настройка редакторов**: скрипт `install.sh` теперь автоматически устанавливает подсветку синтаксиса `foxlang.xml` и настраивает LSP для Kate, а также подготавливает расширение для VS Code.
- **Удобная загрузка из релизов**: в артефакты каждого релиза добавлены отдельные готовые файлы: `foxlang.xml` (для Kate) и `foxlang-vscode.zip` (для VS Code).
- **Обновление документации**: в главном `README.md`, `docs/EDITORS.md` и руководствах редакторов добавлены прямые ссылки на конфигурационные файлы и подробные пошаговые инструкции.

---

# FoxLang 5.5.0 — Changelog

- **Архитектурный рефакторинг ядра**: разделение на переиспользуемую статическую библиотеку `foxlang_core` и тонкий CLI-интерпретатор `foxlang`.
- **Языковой сервер (LSP)**: добавлен автономный сервер `foxlang-lsp` с реализацией протокола JSON-RPC 2.0 (stdio) без внешних зависимостей.
- **Статический семантический анализ**: реализован `SemanticAnalyzer`, выполняющий проверку типов, областей видимости и валидацию AST без исполнения пользовательского кода.
- **Инструменты разработки (IDE & редакторы)**:
  - Поддержка **Kate (KDE)**: XML-схема подсветки синтаксиса KSyntaxHighlighting и конфигурация для плагина LSP Client.
  - Расширение для **Visual Studio Code**: TextMate-грамматика синтаксиса, конфигурация языка и LSP-клиент.
  - Подробное руководство по интеграции в `docs/EDITORS.md`.
- **Точные координаты исходного кода**: поддержка `SourcePosition`, `SourceRange`, учет UTF-16 code units для корректной работы с эмодзи и кириллицей в LSP.
- **Диагностика**: расширенная программная модель `Diagnostic` с уровнями важности (Error, Warning, Info, Hint) и восстановлением после синтаксических ошибок в парсере.
- **Инсталляция и CI**: расширен `install.sh` для установки `foxlang-lsp`; автоматическая сборка релизных бинарников для Linux и Windows в GitHub Actions.

---

# FoxLang 5.4.5 — Changelog

- JSON parser теперь декодирует `\\uXXXX` в UTF-8, включая surrogate pairs для emoji.
- Добавлен env-флаг `FOXLANG_LOG`: `false`, `0`, `off` или `no` отключают std/log; по умолчанию логи включены.

---

# FoxLang 5.4.4 — Changelog

- Исправлен HTTP POST: JSON body теперь безопасно передаётся curl без разрушения кавычек shell.
- POST получил таймауты, fail-with-body и подробное логирование ошибок curl/HTTP.

---

# FoxLang 5.4.3 — Changelog

- Исправлен конфликт ключевого слова `get` с функцией `get()` из `std/server.fox`.
- Проверены lexer keyword tokens: каждый оставшийся специальный токен имеет обработчик parser.

---

# FoxLang 5.4.1 — Changelog

- JSON helper теперь понимает вложенные пути вроде `message.chat.id` и `message.from.username`.
- Добавлено безопасное экранирование JSON-строк.
- Telegram webhook example стал полноценным ботом: /start, /help, /ping, /about, /id и echo.
- Добавлено подробное логирование входящих update и ответов Telegram API.

---

# FoxLang 5.4.0 — Changelog

- Заглушки HTTP-сервера заменены реальным Linux/POSIX HTTP/webhook runtime.
- Добавлены реальные GET/POST routes, request body/method/path и HTTP response status.
- Добавлен модуль `std/server.fox`.
- Добавлена автоматическая загрузка `.env` и обязательные секреты через `secret()`.
- Добавлен пример Telegram webhook server.
- Исправлено отображение exit code curl на POSIX.

---

# FoxLang 5.3.0 — Changelog

- Исправлен разбор `round()` и `random()`: теперь это обычные runtime-функции, а не зарезервированные токены без рабочего parser path.
- Добавлены стандартные модули `log` и `env`.
- Добавлены `log_debug`, `log_info`, `log_warn`, `log_error` и `env_get`.
- Добавлены core/stdlib smoke-тесты в CI; сломанный runtime больше не должен автоматически уходить в Release.
- Добавлен диагностический пример Telegram Bot API без токена в исходнике.
- Сетевые ошибки HTTP GET теперь видимы в stderr и ограничены таймаутами.

---

# FoxLang 5.2.2 — Changelog

- HTTP GET теперь показывает ошибки curl в stderr вместо молчаливого возврата пустой строки.
- Добавлены таймаут подключения и общий таймаут HTTP-запроса.
- Исправлена русская документация стандартной библиотеки.

---

# FoxLang 5.2.1 — Changelog

- README полностью переработан на русском и приведён к актуальной структуре 5.2.
- Linux Release теперь содержит полноценный пользовательский установщик, бинарник и `std/`.
- Установщик размещает runtime в `~/.local/share/foxlang` и команду в `~/.local/bin/foxlang`.
- Добавлен скрипт удаления установленной версии.
- Release workflow упаковывает готовый Linux SDK/runtime вместо одиночного бинарника.

---

# FoxLang 5.2.0 - Changelog

## Standard library
- Added first-class `std/` layout with `terminal`, `net`, `http`, `math`, `string`, and `time`.
- `using module;` now resolves real FoxLang modules instead of being a no-op.
- Added `FOXLANG_HOME` lookup and import deduplication.

## Networking
- Added real POSIX DNS lookup and TCP client primitives.
- Added FoxLang wrappers: `connect_tcp`, `send_tcp`, `recv_tcp`, `close_tcp`, and `resolve_host`.
- Kept HTTPS HTTP helpers for compatibility.
- Documentation now explicitly marks the old server API as compatibility shims instead of claiming it is a real background server.

## Runtime
- Fixed Linux `kbhit()` so it no longer blocks waiting for input.
- Added reusable terminal primitives.
- Added `abs`, `min`, `max`, `clamp`, and `time_ms` runtime helpers.

## Examples
- Added `examples/tcp_client.fox`.
- Added `examples/stdlib_demo.fox`.

---

# FoxLang 5.0.1 - Changelog

## 🆕 Новые возможности

### Встроенная функция read_file()
- **Чтение файлов:** Новая функция `read_file(filename)` для чтения текстовых файлов
- **Умная обработка:** Автоматически пропускает комментарии (строки начинающиеся с #) и пустые строки
- **Безопасность:** Возвращает пустую строку при ошибке чтения файла
- **Применение:** Идеально подходит для чтения конфигурационных файлов

### Встроенная функция http_get()
- **HTTP запросы:** Новая функция `http_get(url)` для выполнения GET запросов
- **Простота использования:** Один вызов функции для получения данных с сервера
- **Интеграция:** Использует системный curl для надежности
- **Применение:** Работа с API, загрузка данных, веб-скрапинг

```cpp
string config = read_file("config.txt");
if (config != "") {
    print("Config loaded: " + config);
}
```

---

# FoxLang 5.0.0 - Changelog

## 🚀 Крупные нововведения

### 1. Пользовательские функции
- **Полная поддержка функций** с параметрами и возвратом значений
- **Локальные области видимости** для параметров функций
- **Рекурсивные вызовы** функций
- **Поддержка всех типов** в параметрах и возврате (`int`, `float`, `string`, `bool`, `void`)

```cpp
int factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}
```

### 2. Модульная система с пользовательскими функциями
- **Библиотеки с функциями**: Теперь `include("lib.fox")` поддерживает пользовательские функции
- **Правильная область видимости**: Функции из библиотек доступны в основном коде
- **Режим импорта**: Функции регистрируются, но код библиотеки не выполняется

```cpp
// math_lib.fox
int add(int a, int b) {
    return a + b;
}

// main.fox
include("math_lib.fox");
int result = add(5, 3); // Работает!
```

### 3. Современный синтаксис идентификаторов
- **Поддержка подчеркиваний**: `user_name`, `get_public_ip`, `my_function`
- **Исправлен лексер**: Идентификаторы с `_` не конфликтуют с ключевыми словами
- **Совместимость**: Все существующие имена переменных продолжают работать

### 4. Расширенная поддержка циклов
- **Цикл for**: Полная поддержка `for (init; condition; step) { ... }`
- **Улучшенные while циклы**: Лучшая обработка условий
- **Вложенные циклы**: Поддержка любого уровня вложенности

## 🔧 Технические улучшения

### Парсер и AST
- **Новые узлы**: `FuncDefNode`, `ReturnNode`, `ForNode`
- **Улучшенная обработка**: `return` внутри функций
- **Контекст функций**: Изолированные области видимости
- **Обработка ошибок**: Лучшие сообщения об ошибках

### Лексер
- **Исправлена логика**: Идентификаторы с подчеркиваниями обрабатываются корректно
- **Приоритет ключевых слов**: Только полные совпадения считаются ключевыми словами

## 📋 Примеры использования

### Пользовательские функции
```cpp
string greet(string name, int age) {
    return "Hello, " + name + "! You are " + age + " years old.";
}

void main() {
    string message = greet("Alice", 25);
    print(message);
}

main();
```

### Библиотеки с функциями
```cpp
// utils.fox
int max(int a, int b) {
    if (a > b) return a;
    return b;
}

void print_array(array arr, int size) {
    int i = 0;
    while (i < size) {
        print("arr[" + i + "] = " + get(arr, i));
        i = i + 1;
    }
}

// main.fox
include("utils.fox");

int maximum = max(10, 20);
array numbers 3;
set(numbers, 0, 5);
set(numbers, 1, 10);
set(numbers, 2, 15);
print_array(numbers, 3);
```

### Современный синтаксис
```cpp
string user_name = "john_doe";
int user_age = 25;
bool is_admin = false;

string get_user_info() {
    return "User: " + user_name + ", Age: " + user_age;
}

void check_permissions() {
    if (is_admin) {
        print("Admin access granted");
    } else {
        print("Regular user access");
    }
}
```

## 🔄 Обратная совместимость
- Все скрипты FoxLang 4.x продолжают работать
- Встроенные функции (`print`, `input`, `fox`) не изменились
- Синтаксис массивов и переменных остался прежним

## 🐛 Исправленные ошибки
- Исправлена обработка идентификаторов с подчеркиваниями
- Устранены конфликты ключевых слов с именами функций
- Улучшена стабильность парсера при обработке функций
- Исправлены ошибки области видимости переменных

---

**Версия 5.0.0** представляет собой значительный шаг вперед в развитии FoxLang, превращая его в полноценный язык программирования с поддержкой пользовательских функций и современной модульной системы.
