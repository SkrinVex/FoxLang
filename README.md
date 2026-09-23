# 🦊 FoxLang

![Версия](https://img.shields.io/badge/version-5.5.3-orange)
![C++](https://img.shields.io/badge/runtime-C%2B%2B17-blue)
![Платформы](https://img.shields.io/badge/platform-Linux%20%7C%20Windows-lightgrey)
![Лицензия](https://img.shields.io/badge/license-MIT-green)

**FoxLang** — встраиваемый и интерпретируемый язык программирования общего назначения с понятным C-подобным синтаксисом, строгими типами, функциями, массивами, модулями, стандартной библиотекой и сетевыми возможностями.

```cpp
// hello.fox
void main() {
    print("Hello from FoxLang!");
}

main();
```

## Установка

Для сборки FoxLang из этого репозитория нужны CMake и компилятор C++17:

```bash
git clone https://github.com/SkrinVex/FoxLang.git
cd FoxLang
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build --config Release
cmake --install build --prefix ./install
```

Добавьте `install/bin` в `PATH` или запускайте `install/bin/foxlang` напрямую
(на Windows — `install/bin/foxlang.exe`). Готовые пакеты опубликованных версий
находятся в [GitHub Releases](https://github.com/SkrinVex/FoxLang/releases).
Наличие новой команды в установленной версии можно проверить через `foxlang --help`.

## Запуск программы

```bash
foxlang hello.fox
```

Это обычный запуск исходника: на этом компьютере нужны FoxLang и файл `hello.fox`.

## Standalone приложения

```bash
foxlang build hello.fox -o hello
```

Запуск на Linux:

```bash
./hello
```

Запуск на Windows:

```powershell
.\hello.exe
```

**Получателю программы не требуется устанавливать FoxLang.** Передайте ему один
executable для той же ОС и архитектуры. Внутри находятся FoxLang runtime,
программа и её FoxLang-модули. Исходный `.fox`, CMake, C++ compiler и отдельный
FoxLang runtime получателю не нужны. Сам `foxlang build` тоже работает без компилятора.

`foxlang build hello.fox` создаёт `hello` (Linux) или `hello.exe` (Windows)
в текущем каталоге. Поддерживается `--output`; существующий выходной файл
не перезаписывается. Linux создаёт Linux executable, Windows — Windows executable.

Это **упаковка со встроенным интерпретатором**, а не AOT-компиляция исходника
непосредственно в машинный код. Стандартная библиотека встроена в FoxLang;
локальные `using` и `include` собираются рекурсивно.

`.env`, значения окружения при сборке и обычные ресурсы **не встраиваются**.
Передавайте секреты через переменные окружения при запуске. Standalone не загружает
`.env` автоматически. Секрет, записанный прямо в `.fox`, останется частью программы.
Файлы для `read_file()` по-прежнему предоставляются отдельно.

HTTP-клиент по-прежнему требует внешний `curl` в `PATH` (для HTTPS — также доверенные
сертификаты ОС). HTTP-сервер и TCP/DNS доступны на Linux/POSIX; в Windows их
реализация пока отсутствует. На Linux сохраняется зависимость от совместимых
системных libc/libm и загрузчика; перенос между glibc и musl не гарантируется.

Подробности: [standalone и ограничения](docs/STANDALONE.md),
[полная документация языка](DOCUMENTATION.md).

## Платформы и проверки

| Платформа | CLI | Standalone build |
|---|---|---|
| Linux x86_64 | Проверено локально, CTest | Проверено из чистого временного каталога |
| Windows x86_64 | Job `desktop-windows` в CI | Job `desktop-windows`: сборка и запуск `.exe` в чистом каталоге; результат текущего изменения ещё требует проверки |
| Android | Планируется отдельная интеграция | Планируется, не реализовано |

Тесты Linux и Windows находятся в [обычном CI](.github/workflows/ci.yml).
Wine/MinGW не заменяют проверку на Windows runner.

---

## Возможности

- Типы данных: `int`, `float`, `string`, `bool`, `void`.
- Пользовательские функции, параметры и `return`, рекурсия.
- Управляющие конструкции: `if / else`, `while`, `for`, `switch / case / default`, `break`, `continue`.
- Массивы и функции работы с ними: `array`, `set`, `get`, `size`.
- Операторы: арифметические, сравнения, составные присваивания (`+=`, `-=`, `*=`, `/=`), инкремент `++`.
- Логические операторы: `&&`, `||`, `!`.
- Модульная система: `include("file.fox")` и стандартная библиотека через `using module;`.
- Предотвращение повторной загрузки модулей.
- Полноценная работа с JSON (вложенные пути `message.chat.id`, Unicode `\uXXXX`, суррогатные пары UTF-16 для emoji, экранирование `json_safe`).
- Чтение переменных окружения `env()` и строгие обязательные секреты `secret()`.
- Автоматическая загрузка конфигурации из `.env`.
- Уровневое логирование (`debug`, `info`, `warn`, `error`, `off`).
- Файловый ввод-вывод (`read_file`, `write_file`, `append_file`).
- Терминальный/TUI API (ANSI-цвета, позиционирование курсора, очистка).
- Сетевой клиент (DNS, TCP-сокеты, HTTP GET/POST/PUT/DELETE).
- HTTP/webhook-сервер на POSIX (`get`, `post`, `body`, `method`, `path`, `respond`, `listen`, `server_stop`).
- Независимая C++17 библиотека ядра (`foxlang_core`) для встраивания в приложения и тесты.
- Полнофункциональный языковой сервер `foxlang-lsp` (LSP 3.17) и готовые плагины для VS Code, Kate и Zed IDE.

---

## Сборка и тестирование (CMake)

Проект использует стандартную систему сборки **CMake** (требуется C++17) и **CTest** для автоматического запуска регрессионных и модульных тестов.

### Быстрая сборка

```bash
git clone https://github.com/SkrinVex/FoxLang.git
cd FoxLang
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Запуск тестов

```bash
ctest --test-dir build -C Release --output-on-failure
```

Все тесты (native unit-тесты Lexer, Parser, Interpreter API, регрессионные тесты языка, локальные тесты HTTP сервера/клиента) запускаются в едином цикле CTest и завершаются с ненулевым статусом при обнаружении регрессий.

Стандартная библиотека встраивается во время конфигурации CMake. Для тестов
дополнительно нужен Python 3; в готовых CLI и standalone он не используется.

---

## Использование CLI

Команда `foxlang` — это тонкая консольная обёртка над ядром `foxlang_core`.

```bash
# Запуск программы
foxlang program.fox

# Просмотр версии
foxlang --version
foxlang -v

# Справка
foxlang --help
foxlang -h
```

Коды возврата:
- `0` — успешное завершение;
- `1` — синтаксическая ошибка, ошибка выполнения (Runtime Error), отсутствие обязательного секрета или файла.

---

## 🐳 Использование в Docker

FoxLang поставляется в виде легковесного контейнера на базе Alpine Linux с предустановленным интерпретатором `foxlang`, языковым сервером `foxlang-lsp` и модулями стандартной библиотеки в `/usr/local/share/foxlang/std`.

Образы автоматически собираются и публикуются в **GitHub Container Registry (GHCR)** при каждом коммите и релизе:
```bash
ghcr.io/skrinvex/foxlang:latest
```

### Быстрый запуск и проверка версии

```bash
docker run --rm ghcr.io/skrinvex/foxlang:latest --version
```

### Запуск локального скрипта через Volume

Смонтируйте текущую директорию с исходным кодом в контейнер:

```bash
docker run --rm -v $(pwd):/app ghcr.io/skrinvex/foxlang:latest script.fox
```

### Запуск веб-сервера / Telegram-бота в фоне

Для запуска сетевых приложений пробросьте порт и передайте переменные окружения / секреты:

```bash
docker run -d \
  --name foxbot \
  --restart unless-stopped \
  -p 8080:8080 \
  -e TELEGRAM_BOT_TOKEN="123456:ABC-DEF..." \
  -e PORT="8080" \
  -v $(pwd):/app \
  ghcr.io/skrinvex/foxlang:latest \
  bot.fox
```

Просмотр логов:
```bash
docker logs -f foxbot
```

### Использование с Docker Compose

Пример `docker-compose.yml` для развёртывания сервиса:

```yaml
version: '3.8'

services:
  foxbot:
    image: ghcr.io/skrinvex/foxlang:latest
    container_name: foxbot
    restart: unless-stopped
    ports:
      - "8080:8080"
    environment:
      - TELEGRAM_BOT_TOKEN=${TELEGRAM_BOT_TOKEN}
      - PORT=8080
    volumes:
      - .:/app
    command: ["bot.fox"]
```

Запуск:
```bash
docker compose up -d
```

### Локальная сборка образа

Вы можете собрать образ локально из исходников:

```bash
docker build -t foxlang:latest .
```

---

## 💻 Поддержка редакторов и IDE (VS Code, Kate, Zed)

Для языка FoxLang доступен автономный языковой сервер `foxlang-lsp` (Language Server Protocol 3.17) и готовые конфигурации для популярных редакторов:
* **Подсветка синтаксиса**: распознавание файлов `.fox`, подсветка ключевых слов, типов, строк с экранированием, чисел и встроенных функций.
* **Статический анализ кода в реальном времени**: обнаружение необъявленных переменных и функций, проверка количества аргументов и корректности возвратов без исполнения программы.
* **Автодополнение (Autocomplete)**: контекстный выбор функций стандартной библиотеки, ключевых слов и локальных переменных.
* **Подсказки при наведении (Hover)**: всплывающее окно с сигнатурами функций и типами идентификаторов.
* **Переход к определению (Go to Definition)**: навигация к месту объявления по `F12` или `Ctrl+Click`.
* **Символы документа (Document Symbols)**: аутлайн функций и переменных файла.

### 🐧 Настройка текстового редактора Kate (KDE)
* **Автоматически**: при запуске скрипта установки `install.sh` подсветка синтаксиса и настройки LSP устанавливаются в систему автоматически!
* **Вручную**:
  1. Скопируйте схему подсветки [`editors/kate/foxlang.xml`](editors/kate/foxlang.xml) в каталог:
     ```bash
     mkdir -p ~/.local/share/org.kde.syntax-highlighting/syntax/
     cp editors/kate/foxlang.xml ~/.local/share/org.kde.syntax-highlighting/syntax/
     ```
  2. Включите плагин **Клиент LSP** в меню Kate (*Настройка → Настроить Kate... → Модули → Клиент LSP*).
  3. Конфигурация для `foxlang-lsp` находится в [`editors/kate/settings.json`](editors/kate/settings.json) (путь: `~/.config/kate/lspclient/settings.json`).

Файл подсветки также можно скачать напрямую из релизов: [`foxlang.xml`](https://github.com/SkrinVex/FoxLang/releases/latest/download/foxlang.xml).

### 🟦 Настройка Visual Studio Code
Исходный код расширения расположен в каталоге [`editors/vscode/`](editors/vscode/):
* [`package.json`](editors/vscode/package.json) — манифест расширения и регистрация языка `.fox`;
* [`syntaxes/foxlang.tmLanguage.json`](editors/vscode/syntaxes/foxlang.tmLanguage.json) — правила TextMate для подсветки синтаксиса;
* [`language-configuration.json`](editors/vscode/language-configuration.json) — автозакрытие скобок, кавычек и правила комментариев;
* [`client/extension.js`](editors/vscode/client/extension.js) — клиент LSP.

**Быстрая установка:**
* **Через установщик**: скрипт `install.sh` автоматически регистрирует расширение в VS Code / VSCodium / Flatpak.
* **Через пакет VSIX**: скачайте [`foxlang-5.5.3.vsix`](https://github.com/SkrinVex/FoxLang/releases/latest/download/foxlang-5.5.3.vsix) и выполните:
  ```bash
  code --install-extension foxlang-5.5.3.vsix
  ```
  *(или выберите в VS Code: Расширения `Ctrl+Shift+X` → `...` → **Install from VSIX...**)*
* **Вручную из репозитория**:
  ```bash
  mkdir -p ~/.vscode/extensions/SkrinVex.foxlang-language-5.5.3
  cp -R editors/vscode/* ~/.vscode/extensions/SkrinVex.foxlang-language-5.5.3/
  ```
*(Расширение полностью автономно и не требует запуска `npm install`)*

### ⚡ Настройка Zed IDE
Исходный код расширения для Zed расположен в [`editors/zed/`](editors/zed/):
* [`extension.toml`](editors/zed/extension.toml) — манифест расширения Zed и подключение Tree-sitter грамматики;
* [`languages/foxlang/config.toml`](editors/zed/languages/foxlang/config.toml) — конфигурация языка, комментариев и скобок;
* [`languages/foxlang/highlights.scm`](editors/zed/languages/foxlang/highlights.scm) — правила подсветки синтаксиса Tree-sitter;
* [`src/lib.rs`](editors/zed/src/lib.rs) — интеграция с `foxlang-lsp` через Zed Extension API (WASM).

**Быстрая установка:**
* **Dev-расширение в Zed**: в палитре команд `Ctrl+Shift+P` вызовите `zed: install dev extension` и укажите путь к [`editors/zed/`](editors/zed/).
* **Через конфигурацию `settings.json`**: добавьте сервер `foxlang-lsp` в `~/.config/zed/settings.json` (подробнее см. [`docs/EDITORS.md`](docs/EDITORS.md)).

📖 Полное руководство по архитектуре, диагностикам и отладке LSP: [`docs/EDITORS.md`](docs/EDITORS.md).

---

## Встраивание FoxLang в C++ приложения

Благодаря разделению архитектуры, FoxLang можно использовать как библиотеку в настольных приложениях, бэкендах, игровых движках и через JNI в Android:

```cpp
#include <foxlang/FoxLang.h>
#include <iostream>

int main() {
    foxlang::Interpreter interpreter;

    // Выполнение кода из строки
    foxlang::RunResult res = interpreter.runSource("int a = 10; int b = 32; int c = a + b;");
    if (res.success) {
        std::cout << "Результат c = " << interpreter.getGlobal("c").value << std::endl;
    } else {
        std::cerr << "Ошибка: " << res.errorMessage << std::endl;
    }

    // Выполнение файла
    foxlang::RunResult fileRes = interpreter.runFile("script.fox");
    return fileRes.exitCode;
}
```

---

## Архитектура репозитория

```text
FoxLang/
├── CMakeLists.txt              # Корневой файл сборки CMake
├── VERSION                     # Версия выпуска
├── include/
│   └── foxlang/                # Публичные C++ заголовочные файлы
│       ├── FoxLang.h           # Публичный API: Interpreter, RunResult, Options
│       ├── SourceLocation.h    # Позиции (UTF-8/UTF-16), диапазоны и модель Diagnostic
│       ├── SemanticAnalyzer.h  # Статический семантический анализ, области видимости
│       ├── Context.h           # Контекст переменных, функций и массивов
│       ├── AST.h               # Чистые узлы AST с диапазонами SourceRange
│       ├── Lexer.h             # Лексический анализатор с отслеживанием UTF-16
│       ├── Parser.h            # Синтаксический анализатор (формирует AST)
│       ├── SourceProvider.h    # Общий доступ к исходникам на диске или в памяти
│       ├── Runtime.h           # Встроенные функции, JSON, логирование, .env
│       ├── Platform.h          # Изоляция платформозависимого кода (POSIX/Windows)
│       └── Token.h             # Определение токенов и позиций
├── src/
│   ├── core/                   # Реализация библиотеки ядра (foxlang_core)
│   │   ├── Context.cpp
│   │   ├── Platform.cpp
│   │   ├── Runtime.cpp
│   │   ├── Lexer.cpp
│   │   ├── Parser.cpp
│   │   ├── SemanticAnalyzer.cpp
│   │   └── Interpreter.cpp
│   ├── cli/                    # Тонкий исполняемый файл CLI (foxlang)
│   │   └── main.cpp
│   ├── standalone/             # Bundle, проверка ELF/PE, упаковка без компилятора
│   └── lsp/                    # Языковой сервер Language Server Protocol (foxlang-lsp)
│       ├── Json.h / Json.cpp
│       ├── Transport.h / Transport.cpp
│       ├── Protocol.h
│       ├── DocumentManager.h / DocumentManager.cpp
│       ├── LspServer.h / LspServer.cpp
│       └── main.cpp
├── editors/                    # Интеграции для редакторов кода
│   ├── vscode/                 # Расширение Visual Studio Code
│   │   ├── package.json
│   │   ├── language-configuration.json
│   │   ├── syntaxes/foxlang.tmLanguage.json
│   │   └── client/extension.js
│   └── kate/                   # Поддержка KDE Kate
│       ├── foxlang.xml         # Подсветка синтаксиса KSyntaxHighlighting
│       └── settings.json       # Конфигурация LSP Client
├── docs/
│   └── EDITORS.md              # Подробное руководство по настройке редакторов
├── std/                        # Стандартная библиотека FoxLang
│   ├── env.fox
│   ├── http.fox
│   ├── json.fox
│   ├── log.fox
│   ├── math.fox
│   ├── net.fox
│   ├── server.fox
│   ├── string.fox
│   ├── terminal.fox
│   └── time.fox
├── tests/                      # Набор тестов (CTest)
│   ├── CMakeLists.txt
│   ├── unit/                   # Native C++ unit-тесты
│   │   ├── test_lexer.cpp
│   │   ├── test_parser.cpp
│   │   ├── test_interpreter.cpp
│   │   ├── test_positions.cpp
│   │   ├── test_semantic_analyzer.cpp
│   │   └── test_lsp_protocol.cpp
│   └── regression/             # Регрессионные тесты языка и окружения (*.fox, *.sh)
├── examples/                   # Примеры программ и Telegram webhook-бот
├── packaging/                  # Скрипты развёртывания и упаковки
├── DOCUMENTATION.md            # Полная документация синтаксиса и модулей
└── CHANGELOG.md                # История версий
```

---

## Поддержка редакторов (VS Code, Kate, Zed) и Language Server (`foxlang-lsp`)

В FoxLang входит полнофункциональный языковой сервер **`foxlang-lsp`** по протоколу LSP 3.17, работающий через стандартные потоки ввода-вывода (JSON-RPC stdio).

### Возможности:
* **Подсветка синтаксиса**: файлы подсветки TextMate для VS Code, KSyntaxHighlighting XML для Kate и Tree-sitter запросы для Zed IDE.
* **Диагностика ошибок (Diagnostics)**: синтаксические и семантические ошибки с точными позициями в кодовых единицах UTF-16 (корректно поддерживаются кириллица и 4-байтовые эмодзи вроде `🦊`).
* **Автодополнение (Autocomplete)**: ключевые слова языка, функции стандартной библиотеки и пользовательские идентификаторы в текущей области видимости.
* **Подсказки при наведении (Hover)**: всплывающие окна с сигнатурами функций и типами переменных в Markdown.
* **Переход к определению (Go to Definition)**: быстрый переход по `F12` к месту объявления переменной или функции.
* **Символы документа (Document Symbols)**: навигация по функциям и переменным файла.
* **Безопасность**: `SemanticAnalyzer` и `foxlang-lsp` никогда не исполняют пользовательский код для его анализа.

Подробные пошаговые инструкции по подключению см. в [docs/EDITORS.md](docs/EDITORS.md), [editors/vscode/README.md](editors/vscode/README.md), [editors/kate/README.md](editors/kate/README.md) и [editors/zed/README.md](editors/zed/README.md).

---

## Подготовка к Android (JNI)

Архитектура изолирует платформозависимый код в `Platform.h` / `Platform.cpp` и `foxlang_core`. Библиотека может компилироваться в `libfoxlang.so` для Android с помощью Android NDK.

**Текущие особенности и ограничения при встраивании в Android:**
- **Отсутствие утилиты curl**: на обычном Android нет системного бинарника `curl`. При встраивании в Android HTTP-клиент следует перенаправлять в Java-стек (`HttpURLConnection` / `OkHttp`) через JNI или линковать проект с `libcurl.so`.
- **Терминальный ввод**: функции `getch()` и `kbhit()` обращаются к `stdin` TUI, который не поддерживается в графических Android-активностях.
- **Сетевые сокеты**: требуют разрешения `android.permission.INTERNET` в манифесте приложения.
- **Файловая система и FOXLANG_HOME**: на Android путь к стандартной библиотеке `std/` должен указывать на внутренний каталог приложения (например, `/data/data/<package>/files/std`), задаваемый через `InterpreterOptions.foxHome`.
- **Переменные окружения**: Android-приложения не используют глобальные переменные процесса; настройки и секреты передаются через API `Interpreter::setGlobal`.

---

## Стандартная библиотека

Подключение стандартных библиотек осуществляется через `using <модуль>;`:

```cpp
using math;
using string;
using time;
using log;
using env;
using json;
using server;
using net;
using http;
using terminal;
```

Для `using` проверяются `std/<модуль>.fox`, затем `<модуль>.fox`: каждый путь
ищется относительно импортирующего файла, текущего каталога и `FOXLANG_HOME`.
Если файлов нет, используется встроенная stdlib. При упаковке применяется тот же
порядок; при запуске standalone используются только уже упакованные модули.

---

## Telegram Webhook-бот на FoxLang

Пример рабочего webhook-сервера доступен в [`examples/telegram_webhook.fox`](examples/telegram_webhook.fox):

```cpp
using server;
using http;
using env;
using log;
using json;

string token = secret("TELEGRAM_BOT_TOKEN");
string api = "https://api.telegram.org/bot" + token + "/";

void health() {
    respond("{\"ok\":true,\"service\":\"foxbot\"}");
}

void webhook() {
    string update = body();
    string chat_id = json_path(update, "message.chat.id");
    string text = json_path(update, "message.text");
    info("Сообщение от " + chat_id + ": " + text);
    respond("{\"ok\":true}");
}

void main() {
    get("/health", "health");
    post("/telegram", "webhook");
    listen(8080);
}

main();
```

---

## Документация и ссылки

- Подробное руководство по языку: [DOCUMENTATION.md](DOCUMENTATION.md)
- История версий: [CHANGELOG.md](CHANGELOG.md)
- Репозиторий: [https://github.com/SkrinVex/FoxLang](https://github.com/SkrinVex/FoxLang)

## Лицензия

FoxLang распространяется под лицензией MIT.

**Автор:** [SkrinVex](https://skrinvex.su)
