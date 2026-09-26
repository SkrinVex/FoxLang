# Тестирование FoxLang

Все тесты запускаются одним CTest и завершаются ненулевым статусом при регрессии.
Сетевые тесты работают только с localhost.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

| Группа | Где | Что проверяет |
|---|---|---|
| `unit_*` | `tests/unit/*.cpp` | лексер (токены, escape `\u`), парсер (грамматика, восстановление после ошибок), интерпретатор (типы, массивы и их время жизни, `exit`, аргументы, каталог встроенных функций), анализатор, позиции UTF-16, протокол LSP, графика, формат bundle, CA |
| `regression_*` | `tests/regression/*.fox` | язык целиком: синтаксис и типы, выражения, области видимости, управление потоком, функции, массивы, строки, математика, модули, JSON, файлы, окружение, аргументы и время |
| `smoke_*` | `tests/*.fox` | короткие сквозные сценарии |
| `integration_*` | `tests/regression/*.sh` | сообщения об ошибках и коды возврата, уровни логов, локальный HTTP-сервер и клиент (Linux) |
| `check_examples_*`, `check_std_*` | `examples/`, `std/` | каждый пример и модуль проходит `foxlang check` |
| `docs_examples` | `tests/test_docs.py` | каждый блок кода FoxLang в README и docs проходит `foxlang check`; DOCUMENTATION.md описывает каждую встроенную функцию и каждую функцию `std` |
| `unit_web` | `tests/unit/test_web.cpp` | шаблоны, `json_set`, URL/HTML-кодирование, формы, multipart, cookies, маршруты с параметрами, статика и защита от `..`, 404/405, CORS — без сети |
| `web_server_example` | `tests/test_web_server.py` | сайт `examples/website` по настоящему HTTP: шаблоны, форма, cookie, редирект, загрузка и скачивание, статика, HEAD, журнал запросов |
| `lsp_project` | `tests/test_lsp_project.py` | проект из нескольких файлов в папке с кириллицей: соседние файлы видят друг друга, независимые программы — нет, переход к определению в другой файл, обновление диагностики после правки |
| `lsp_catalog` | `tests/test_lsp_catalog.py` | настоящий `foxlang-lsp`: completion, hover и signatureHelp всех встроенных функций и всех функций 14 модулей |
| `packaging_versions` | `tests/test_versions.py` | манифесты редакторов совпадают с `VERSION`, номер версии не продублирован в документации, VSIX собирается правильно, подсветка синхронизирована с каталогом функций |
| `standalone_*` | `tests/standalone/` | упаковка и запуск приложений в чистом каталоге (см. ниже) |

Регрессионные тесты на FoxLang завершаются через `exit(1)` при первой неудачной
проверке и печатают `FAIL: <описание>`.

## Standalone

Для интеграционных тестов нужны Python 3 и утилита `openssl` для временных тестовых
сертификатов. На Windows подходит OpenSSL из Git for Windows. Готовым программам
эти инструменты не требуются.

```bash
ctest --test-dir build -C Release -L standalone --output-on-failure
ctest --test-dir build -C Release -R standalone_hello -V
```

`standalone_bundle_format` проверяет сериализацию, границы, версии, CRC32,
повреждённые ELF/PE-структуры и усечения payload. Остальные standalone-тесты
копируют CLI без stdlib во временный каталог, собирают fixtures, переносят только
готовый executable получателю и удаляют исходники вместе с копией CLI.

Проверяются точный stdout, ошибочные аргументы, все std-модули, рекурсивные и
циклические локальные импорты, JSON/кириллица/emoji, переменные окружения во время
запуска, отсутствие `.env` и ресурсов в bundle, ошибки runtime и повреждение
готового executable. Сетевые тесты используют только localhost и пустой PATH:
HTTP GET/POST/PUT/DELETE с Unicode и shell-символами, HTTPS с доверенным и
недоверенным сертификатом и ошибочным именем хоста, HTTP-сервер/webhook, DNS и TCP.
Для сервера проверяются HTTP 500 при исключении обработчика и отказ на неверный
Content-Length. Эти сценарии входят в Linux и Windows CI.
Hello-тест выводит размер executable в байтах.

`standalone_https_server` проверяет HTTPS/webhook без reverse proxy: внешние
runtime credentials, отказ при отсутствующем/несовпадающем ключе, неверном
сертификате и имени хоста, отказ от plaintext, большие Unicode-сообщения через
несколько TLS records, HTTP 500 и остановку. Ключи генерируются временно и не
попадают в bundle. `unit_certificates` разбирает все 121 встроенный CA snapshot.

`portable-linux` собирает статический runtime в Alpine, выполняет там весь CTest,
проверяет отсутствие ELF interpreter/shared libraries и запускает standalone-набор
на Ubuntu. Локально: `python3 tests/standalone/test_portable_linux.py build-portable/foxlang`
после экспорта Docker target `portable` (команда приведена в README).

`packaging_versions` сверяет VERSION с манифестами редакторов, проверяет, что
документация и примеры не повторяют номер версии, создаёт VSIX и проверяет отказ
при несовпадении версий.

## Песочница в браузере

`tests/web/test_playground.js` запускает WebAssembly-сборку песочницы в Node: все
примеры страницы, эхо ввода, строку ошибки, предел глубины вызовов, сообщение о
недоступной графике и проверку кода для редактора, а для сборки с окном — кадры на
холсте страницы, клавиши во время `window_poll()` и перехват ошибок. Задача `web` в CI собирает её
Emscripten и запускает этот тест.

## Нативная графика

`unit_graphics` проверяет программный рендерер и ошибки API без окна.
`standalone_graphics` открывает настоящее окно из упакованного executable,
проверяет пиксели и ввод ОС. На Linux нужны DISPLAY и libX11 для тестового
драйвера; для CI используется `xvfb-run -a ctest --test-dir build`.
Без DISPLAY тест пропускается, если не задано `FOXLANG_REQUIRE_GRAPHICS_TESTS=1`.
Windows использует встроенные Win32/GDI без внешнего драйвера тестов.

## LSP и каталоги редакторов

`lsp_catalog` запускает настоящий `foxlang-lsp` по stdio в чистом каталоге без
DISPLAY и stdlib на диске. Проверяет completion/hover/signatureHelp всех 14
std-модулей, каталог встроенных функций, справку ключевых слов и операторов,
неполные вызовы, вложенные выражения, кавычки, комментарии, кириллицу и emoji.
Функции graphics/HTTP/filesystem при этом не выполняются.
`packaging_versions` также проверяет синхронизацию списков функций для подсветки
VS Code, Kate и Zed с runtime и `std/*.fox`.

```bash
ctest --test-dir build -R 'lsp|semantic|packaging_versions' --output-on-failure
```
