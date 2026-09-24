# 🦊 FoxLang

![Версия](https://img.shields.io/github/v/release/SkrinVex/FoxLang?label=version&color=orange)
![C++](https://img.shields.io/badge/runtime-C%2B%2B17-blue)
![Платформы](https://img.shields.io/badge/platform-Linux%20%7C%20Windows-lightgrey)
![Лицензия](https://img.shields.io/badge/license-MIT-green)

**FoxLang** — встраиваемый интерпретируемый язык общего назначения с понятным
C-подобным синтаксисом, строгими типами, массивами, модулями и стандартной
библиотекой для строк, файлов, JSON, HTTP, серверов и 2D-графики. Программу можно
запустить интерпретатором или собрать в один исполняемый файл, которому не нужен
установленный FoxLang.

```cpp
using string;
using arrays;

array words = split("лис волк заяц", " ");
sort(words);
for (int i = 0; i < size(words); i++) {
    print(i + 1, upper(words[i]));
}
```

## Возможности

- Типы `int`, `float`, `string`, `bool`, `array`; одно понятное правило приведения
  типов для переменных, параметров и результатов функций.
- Функции с рекурсией, блочные области видимости, `if`/`while`/`for`/`switch`.
- Динамические массивы: литералы `[1, 2, 3]`, индексы `items[i]`, `push`/`pop`,
  сортировка и срезы; функции принимают и возвращают массивы.
- Строки с полной поддержкой UTF-8: длина, подстроки и поиск считают символы,
  `upper`/`lower` понимают кириллицу.
- Стандартная библиотека из 15 модулей: `string`, `arrays`, `math`, `json`, `fs`,
  `os`, `time`, `env`, `log`, `http`, `server`, `net`, `terminal`, `graphics`, `ui`.
- Веб-сервер для сайтов и API: маршруты с параметрами (`/users/:id`), статические
  файлы, HTML-шаблоны, формы и загрузка файлов, cookies, редиректы, скачивание, CORS,
  HTTPS без reverse proxy. HTTP(S)-клиент со статусом ответа, TCP и DNS — на Linux и
  Windows, без внешних утилит.
- Сборка JSON без склейки строк (`json_set`, `json_value`) и чтение по путям.
- Нативное окно с 2D-рисованием, текстом (строчные и прописные, латиница и кириллица),
  вводом текста в любой раскладке, мышью с колесом и буфером обмена; модуль `ui` с
  кнопками, полями ввода и модальными окнами, которые правильно распределяют щелчки
  и клавиатуру.
- Файловая система целиком: права доступа, время изменения, владелец, копирование и
  перемещение каталогов, работа с путями; запуск программ и открытие файлов
  приложением по умолчанию.
- `foxlang check` — статическая проверка без запуска; языковой сервер
  `foxlang-lsp` с расширениями для VS Code, Kate и Zed. Проект из нескольких файлов
  анализируется целиком, ошибки указывают файл и строку, программу можно запустить
  кнопкой ▶ прямо из редактора.
- `foxlang build` — самостоятельное приложение для Linux или Windows.
- Библиотека `foxlang_core` для встраивания в программы на C++.

Полный справочник языка: [DOCUMENTATION.md](DOCUMENTATION.md).

## Установка

Готовые пакеты для Linux и Windows — в [GitHub Releases](https://github.com/SkrinVex/FoxLang/releases).
Linux-пакет содержит установщик: `./install.sh` кладёт `foxlang` и `foxlang-lsp` в
`~/.local/bin` и настраивает подсветку и языковой сервер для Kate, VS Code и Zed.

Сборка из исходников (CMake 3.18+, компиляторы C и C++17; на Linux ещё pkg-config
и `libxcb1-dev libxau-dev libxdmcp-dev`):

```bash
git clone https://github.com/SkrinVex/FoxLang.git
cd FoxLang
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build --config Release
cmake --install build --prefix ./install
```

Добавьте `install/bin` в `PATH` или запускайте `install/bin/foxlang` напрямую.

## Использование

```bash
foxlang program.fox [аргументы...]   # запуск
foxlang check program.fox            # проверка без запуска
foxlang build program.fox -o app     # самостоятельное приложение
foxlang --version
foxlang --help
```

Код возврата: `0` — успех, `1` — ошибка (синтаксис, тип, выполнение, отсутствующий
модуль или секрет), либо значение, переданное в `exit(code)`.

### Самостоятельные приложения

```bash
foxlang build hello.fox -o hello
./hello          # Linux
.\hello.exe      # Windows
```

Внутри одного исполняемого файла — рантайм FoxLang, программа и все её модули.
Получателю не нужны FoxLang, компилятор или CMake. Это упаковка со встроенным
интерпретатором, а не компиляция в машинный код: исходник можно извлечь. `.env`,
переменные окружения и файлы ресурсов не встраиваются. Linux создаёт Linux-приложения,
Windows — `.exe`. Linux-пакет релизов собран статически на musl и не зависит от
версии glibc. Подробности: [docs/STANDALONE.md](docs/STANDALONE.md).

## Пример: сайт

```cpp
using server;
using json;

void home() {
    respond_html(template_render("<h1>Привет, {{name}}!</h1>", json_set("", "name", query_param("name"))));
}

void user() {
    respond(json_set("", "id", param("id")));
}

static_files("/static", "public");
get("/", "home");
get("/users/:id", "user");
listen(8080);
```

HTTPS включается вызовом `listen_tls(port, certificate, private_key)` без reverse
proxy. Больше примеров — в [examples/](examples): сайт-гостевая книга из нескольких
файлов с шаблонами, формами и загрузкой файлов ([examples/website](examples/website)),
файловый браузер с диалогами ([examples/file_browser.fox](examples/file_browser.fox)),
игра, частотный словарь, список дел в файле, HTTP-клиент, REST API, Telegram-боты на
webhook и long polling, TCP, терминал и графика.

## Платформы

| Платформа | Интерпретатор | Standalone | Проверка |
|---|---|---|---|
| Linux x86_64 | да | да | CI: Ubuntu и статическая сборка в Alpine |
| Windows x86_64 | да | да | CI: MSVC, запуск `.exe` в чистом каталоге |
| Android | ядро собирается NDK | нет | не проверяется |

## Редакторы

`foxlang-lsp` (LSP 3.17) даёт диагностику при вводе, автодополнение, подсказки при
наведении и по параметрам, переход к определению и структуру файла. Описания
встроенных функций и модулей берутся из того же каталога, по которому работает
рантайм, поэтому подсказки всегда совпадают с языком.

* **VS Code** (подсветка, LSP, кнопка ▶ запуска, проверка и сборка): установите `foxlang.vsix` из [последнего выпуска](https://github.com/SkrinVex/FoxLang/releases/latest/download/foxlang.vsix)
  командой `code --install-extension foxlang.vsix`.
* **Kate**: подсветка [`editors/kate/foxlang.xml`](editors/kate/foxlang.xml), настройки
  LSP [`editors/kate/settings.json`](editors/kate/settings.json) и внешние инструменты
  запуска, проверки и сборки [`editors/kate/externaltools/`](editors/kate/externaltools/).
* **Zed**: dev-расширение из [`editors/zed/`](editors/zed/) с задачами запуска.

Пошаговая настройка: [docs/EDITORS.md](docs/EDITORS.md).

## Docker

```bash
docker run --rm ghcr.io/skrinvex/foxlang:latest --version
docker run --rm -v "$(pwd)":/app ghcr.io/skrinvex/foxlang:latest script.fox
```

Образ на Alpine с `foxlang`, `foxlang-lsp` и стандартной библиотекой публикуется
при каждом push в `master` и при каждом выпуске.

## Встраивание в C++

```c++
#include <foxlang/FoxLang.h>
#include <iostream>

int main() {
    foxlang::Interpreter interpreter;
    foxlang::RunResult result = interpreter.runSource("int answer = 40 + 2;");
    if (result.success) std::cout << interpreter.getGlobal("answer").value << std::endl;
    return result.exitCode;
}
```

API описан в [разделе 20 документации](DOCUMENTATION.md#20-встраивание-в-c).

## Разработка

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Тесты охватывают лексер, парсер, интерпретатор, анализатор и LSP (C++), регрессии
языка на FoxLang, ошибки и коды возврата, standalone-приложения, HTTP(S), TCP,
графику, а также проверяют, что каждый пример и каждый блок кода в документации
проходит `foxlang check`. Подробно: [tests/README.md](tests/README.md).

Версия хранится только в файле `VERSION`: CMake передаёт её в исполняемые файлы и
сам проставляет в манифесты расширений редакторов. Изменение `VERSION` в `master`
запускает сборку и публикацию выпуска.

```text
FoxLang/
├── VERSION              # единственное место с номером версии
├── include/foxlang/     # публичный C++ API: Interpreter, Builtins, Parser, SemanticAnalyzer
├── src/core/            # ядро foxlang_core: лексер, парсер, AST, рантайм, сеть
│   └── builtins/        # каталог встроенных функций, по разделам
├── src/graphics/        # программный рендерер и нативные окна
├── src/cli/             # команда foxlang
├── src/standalone/      # упаковка приложений
├── src/lsp/             # языковой сервер foxlang-lsp
├── std/                 # стандартная библиотека на FoxLang
├── examples/            # примеры программ
├── editors/             # VS Code, Kate, Zed
├── tests/               # unit, регрессионные, standalone и LSP-тесты
└── docs/                # руководства: редакторы, графика, standalone
```

## Лицензия

MIT. Автор: [SkrinVex](https://skrinvex.su).
