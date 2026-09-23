# 🦊 FoxLang

![Версия](https://img.shields.io/badge/version-5.4.7-orange)
![C++](https://img.shields.io/badge/runtime-C%2B%2B17-blue)
![Платформы](https://img.shields.io/badge/platform-Linux%20%7C%20Windows-lightgrey)
![Лицензия](https://img.shields.io/badge/license-MIT-green)

**FoxLang** — небольшой интерпретируемый язык программирования общего назначения с понятным C-подобным синтаксисом, строгими типами, функциями, массивами, модулями, стандартной библиотекой и сетевыми возможностями.

## Возможности

- типы `int`, `float`, `string`, `bool`, `void`;
- пользовательские функции, параметры и `return`;
- `if / else`, `while`, `for`, `switch`;
- массивы и функции работы с ними;
- логические операторы `&&`, `||`, `!`;
- подключение файлов через `include("file.fox")`;
- стандартные модули через `using module;`;
- стандартная библиотека `std/`;
- файловый ввод-вывод;
- терминальный/TUI API;
- DNS и настоящие TCP-соединения на POSIX;
- HTTP GET/POST/PUT/DELETE;
- математические и строковые функции;
- Linux и Windows сборки в GitHub Actions.

## Установка

### Linux — готовый релиз

Скачай архив `FoxLang-<версия>-linux-x86_64.tar.gz` из Releases, распакуй и запусти установщик:

```bash
tar -xzf FoxLang-*-linux-x86_64.tar.gz
cd FoxLang-*-linux-x86_64
./install.sh
```

По умолчанию FoxLang устанавливается в `~/.local/share/foxlang`, а команда `foxlang` — в `~/.local/bin`. Root не требуется.

После установки:

```bash
foxlang --version
foxlang hello.fox
```

Удаление:

```bash
~/.local/share/foxlang/uninstall.sh
```

### Сборка из исходников

```bash
g++ -std=c++17 -O2 src/main.cpp src/Lexer.cpp src/Parser.cpp -o foxlang
./foxlang --version
```

## Быстрый старт

Создай `hello.fox`:

```cpp
void main() {
    print("Привет из FoxLang! 🦊");
}

main();
```

Запусти:

```bash
foxlang hello.fox
```

## Стандартная библиотека

FoxLang использует каталог `std/`. Модуль подключается так:

```cpp
using math;
using string;
using time;
using terminal;
using net;
using http;
```

`using net;` сначала ищет `std/net.fox`, затем обычный `net.fox`. При установленной версии путь к стандартной библиотеке автоматически задаётся установщиком через обёртку `foxlang`.

Доступные модули:

| Модуль | Назначение |
| --- | --- |
| `math` | `min`, `max`, `clamp` и математические помощники |
| `string` | поиск, замена и преобразования строк |
| `time` | задержки и время |
| `terminal` | очистка экрана, курсор, цвет и вывод без переноса |
| `net` | DNS и TCP-клиент |
| `http` | HTTP-запросы |

## Сеть

### TCP

```cpp
using net;

void main() {
    string ip = resolve_host("example.com");
    print("IP: " + ip);

    int socket = connect_tcp("example.com", 80);
    if (socket >= 0) {
        send_tcp(socket, "GET / HTTP/1.0\r\nHost: example.com\r\n\r\n");
        string response = recv_tcp(socket, 4096);
        print(response);
        close_tcp(socket);
    }
}

main();
```

### HTTP

```cpp
using http;

string body = http_fetch("https://example.com");
print(body);
```

### HTTP/webhook-сервер

На Linux/POSIX FoxLang содержит встроенный HTTP-сервер для webhook-приложений. Подключи `using server;`, зарегистрируй `get(...)` / `post(...)` и вызови `listen(port)`. Для публичного HTTPS рекомендуется reverse proxy или Cloudflare Tunnel.

Для конфигурации и секретов доступен `using env;`: FoxLang автоматически читает `.env` рядом со скриптом (системные переменные окружения имеют приоритет). `secret("NAME")` завершает программу с ошибкой, если обязательного секрета нет.

Логирование через `using log;` управляется `FOXLANG_LOG_LEVEL=debug|info|warn|error|off`. По умолчанию используется `info`.

## Терминальный API

```cpp
using terminal;

clear();
hide_cursor();
goto_xy(4, 10);
color(36);
write("FoxLang");
reset_color();
show_cursor();
```

На этом API можно делать псевдографические игры и TUI-приложения.

## Структура репозитория

```text
FoxLang/
├── src/          # интерпретатор на C++17
├── std/          # стандартная библиотека FoxLang
├── examples/     # примеры программ
├── test/         # старый набор тестов
├── tests/        # новые CI/smoke-тесты
├── doc/          # дополнительная документация
├── VERSION       # текущая версия
├── DOCUMENTATION.md
└── CHANGELOG.md
```

## Автоматические релизы

Release workflow запускается при изменении файла `VERSION` и сверяет его с `foxlang --version`.

Если тег для этой версии ещё не существует, CI:

1. собирает FoxLang для Linux и Windows;
2. создаёт тег `v<версия>`;
3. формирует Linux-пакет с установщиком и стандартной библиотекой;
4. формирует Windows-архив;
5. автоматически публикует GitHub Release.

Если версия не изменилась и тег уже существует, новый Release не создаётся.

## Документация

Полное описание синтаксиса и встроенных функций находится в [DOCUMENTATION.md](DOCUMENTATION.md). История изменений — в [CHANGELOG.md](CHANGELOG.md).

## Лицензия

FoxLang распространяется по лицензии MIT.

**Автор:** [SkrinVex](https://skrinvex.su)
