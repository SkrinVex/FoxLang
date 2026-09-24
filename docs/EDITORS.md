# Интеграция FoxLang с редакторами кода (VS Code и Kate)

В данном документе описана архитектура инструментов разработчика для языка FoxLang, реализация Language Server Protocol (`foxlang-lsp`), а также пошаговые инструкции по настройке и использованию FoxLang в **Visual Studio Code** и **Kate (KDE)**.

---

## 1. Архитектура языкового инструментария

Архитектура разделения компилятора, статического анализатора и языкового сервера:

```
           .fox исходный код
                   ↓
                 Lexer (отслеживание UTF-8 байт и UTF-16 code units)
                   ↓
                 Parser (восстановление после ошибок + SourceRange на AST-узлах)
                   ↓
                  AST
               ↙       ↘
          Runtime     SemanticAnalyzer (чистый статический обход без исполнения)
                          ↓
                     Diagnostics (структурированные ошибки с точными диапазонами)
                          ↓
                     foxlang-lsp (stdio JSON-RPC 3.17 Language Server)
                     ↙         ↘
               VS Code         Kate
```

### Принципы работы анализатора:
1. **Безопасность и невмешательство**: `SemanticAnalyzer` и `foxlang-lsp` **никогда не выполняют** код программы пользователя для анализа (исключены случайные бесконечные циклы, сетевые запросы или деление на ноль).
2. **Точные позиции в UTF-16**: В соответствии со спецификацией LSP, позиции символов (character offsets) считаются в кодовых единицах UTF-16. Кириллические символы занимают 1 кодовую единицу (хотя в UTF-8 занимают 2 байта), а 4-байтовые эмодзи (например, `🦊`) — 2 кодовые единицы (суррогатная пара).
3. **Восстановление при ошибках (Error Recovery)**: При наличии синтаксической ошибки парсер регистрирует структурированную диагностику, синхронизируется по границе следующей инструкции (statement) и продолжает построение AST для оставшейся части файла, позволяя серверу продолжать отдавать подсказки и автодополнение.
4. **Чистый протокол**: Стандартный поток вывода (`stdout`) зарезервирован исключительно под JSON-RPC сообщения с фреймингом `Content-Length`. Все логи и служебные сообщения направляются только в `stderr`.

---

## 2. Языковой сервер `foxlang-lsp`

Исполняемый файл `foxlang-lsp` собирается вместе с проектом и предоставляет полнофункциональный сервер LSP 3.17:

```bash
# Проверка версии сервера
foxlang-lsp --version
# foxlang-lsp 5.7.0

# Справка по использованию
foxlang-lsp --help
```

### Поддерживаемые методы LSP:
* `initialize` / `initialized`: обмен возможностями (capabilities: hover, signatureHelp, definition, completion, documentSymbol, textDocumentSync).
* `textDocument/didOpen`: разбор документа и немедленная публикация диагностики (`textDocument/publishDiagnostics`).
* `textDocument/didChange`: динамическое обновление документа при редактировании с повторным разбором и обновлением ошибок в реальном времени.
* `textDocument/didClose`: очистка состояния документа из памяти сервера.
* `textDocument/signatureHelp`: интерактивные всплывающие подсказки сигнатуры при вводе `(` или `,` с подсветкой активного параметра (в стиле Kotlin и Zig).
* `textDocument/hover`: всплывающие подсказки с форматированными сигнатурами, типами и Markdown-документацией с примерами.
* `textDocument/completion`: контекстное автодополнение ключевых слов, модулей, функций стандартной библиотеки и переменных.
* `textDocument/definition`: переход к объявлению переменной или функции (`Go to Definition`).
* `textDocument/documentSymbol`: структура/аутлайн файла для быстрой навигации (`Document Symbols`).
* `shutdown` / `exit`: корректное завершение работы сервера.

---

### Подсказки графики и стандартной библиотеки (5.6.1)

После `using graphics;` доступны автодополнение, справка при наведении и подсказки
аргументов всех 15 функций модуля. Например, при вводе `draw_text(` сервер показывает
`draw_text(int x, int y, string text, int scale, int color) -> void` и описание
встроенного шрифта. Сам модуль `graphics` предлагается при дополнении `using`.
Такие же проверки охватывают функции остальных десяти std-модулей и все встроенные
функции runtime. Ключевые слова и операторы также имеют справку при наведении.

Подсказки учитывают позиции UTF-16, вложенные вызовы, запятые в строках,
экранированные кавычки и однострочные комментарии. Анализ никогда не открывает
графические окна, файлы приложения или сетевые соединения.

Обновите **оба** компонента: исполняемый файл `foxlang-lsp` и расширение/подсветку
редактора. Установка одного нового VSIX не обновляет отдельный LSP executable.
После обновления перезапустите редактор или его языковой сервер.

Списки функций для подсветки VS Code, Kate и Zed обновляются командой:

```bash
python3 packaging/sync_editor_builtins.py
python3 packaging/sync_editor_builtins.py --check
```

## 3. Настройка Visual Studio Code

Готовое расширение для VS Code находится в каталоге [`editors/vscode/`](../editors/vscode/).

### Установка:
1. Соберите FoxLang и языковой сервер:
   ```bash
   cmake -S . -B build
   cmake --build build
   ```
2. Скопируйте каталог расширения в папку расширений VS Code:
   ```bash
   cp -r editors/vscode ~/.vscode/extensions/SkrinVex.foxlang-language-5.7.0
   ```
3. Перезапустите VS Code либо вызовите команду `Developer: Reload Window` через палитру команд (`Ctrl+Shift+P`).
4. При открытии любого файла с расширением `.fox` активируется подсветка синтаксиса и автоматически запустится `foxlang-lsp`.

### Конфигурация в `settings.json` VS Code:
```json
{
  "foxlang.lsp.serverPath": "foxlang-lsp",
  "foxlang.lsp.trace.server": "off"
}
```
*Если бинарник `foxlang-lsp` не находится в глобальном `PATH`, укажите абсолютный путь: `"/path/to/FoxLang/build/foxlang-lsp"`.*

---

## 4. Настройка текстового редактора Kate (KDE)

Поддержка редактора Kate находится в каталоге [`editors/kate/`](../editors/kate/).

### Шаг 1. Подсветка синтаксиса (KSyntaxHighlighting)
Скопируйте файл правил `foxlang.xml` в каталог синтаксисов KDE:

```bash
mkdir -p ~/.local/share/org.kde.syntax-highlighting/syntax/
cp editors/kate/foxlang.xml ~/.local/share/org.kde.syntax-highlighting/syntax/
```

Теперь Kate распознаёт файлы `.fox` с подсветкой ключевых слов, типов данных, комментариев, чисел, строк и встроенных функций.

### Шаг 2. Активация плагина LSP Client в Kate
1. Откройте **Kate** -> меню **Settings** (Настройка) -> **Configure Kate...** (Настроить Kate).
2. Перейдите во вкладку **Plugins** (Плагины) и включите флажок **LSP Client** (Клиент LSP). Нажмите **Apply**.

### Шаг 3. Конфигурация сервера в Kate
Добавьте сервер `foxlang` в файл конфигурации `~/.config/kate/lspclient/settings.json`:

```json
{
    "servers": {
        "foxlang": {
            "command": ["foxlang-lsp"],
            "root": "",
            "url": "https://github.com/SkrinVex/FoxLang",
            "highlightingModeRegex": "^FoxLang$"
        }
    }
}
```

*Если `foxlang-lsp` собран локально в проекте и не скопирован в `/usr/local/bin`, укажите полный путь к бинарнику, например:*
```json
{
    "servers": {
        "foxlang": {
            "command": ["/home/user/FoxLang/build/foxlang-lsp"],
            "root": "",
            "url": "https://github.com/SkrinVex/FoxLang",
            "highlightingModeRegex": "^FoxLang$"
        }
    }
}
```

### Возможности в Kate:
* **Подсветка ошибок в коде**: красные и жёлтые подчеркивания в редакторе и список в нижней вкладке **Diagnostics**.
* **Автодополнение**: всплывающий список вариантов при вводе идентификаторов.
* **Наведение курсора**: окно с сигнатурой функции при наведении мыши.
* **Переход к определению**: клавиша `F12` или вызов через контекстное меню.
* **Панель символов**: отображение списка функций и переменных текущего документа.

---

## 5. Настройка редактора Zed IDE

Поддержка редактора Zed IDE находится в каталоге [`editors/zed/`](../editors/zed/).

Расширение обеспечивает:
* Автоматическое распознавание файлов `.fox`;
* Подсветку синтаксиса на основе Tree-sitter ([`highlights.scm`](../editors/zed/languages/foxlang/highlights.scm));
* Автозакрытие и сопоставление скобок ([`brackets.scm`](../editors/zed/languages/foxlang/brackets.scm));
* Интеграцию с `foxlang-lsp` через Rust/WASI расширение ([`src/lib.rs`](../editors/zed/src/lib.rs)).

### Вариант А: Локальная установка dev-расширения в Zed
1. Убедитесь, что бинарник `foxlang-lsp` установлен в системе (входит в PATH).
2. Запустите Zed.
3. Откройте командную палитру: `Ctrl+Shift+P` (Linux/Windows) или `Cmd+Shift+P` (macOS).
4. Выполните команду `zed: install dev extension`.
5. Выберите каталог [`editors/zed`](../editors/zed) из репозитория FoxLang.
6. Zed автоматически скомпилирует расширение и подключит языковой сервер к `.fox` файлам.

### Вариант Б: Быстрое подключение через `settings.json` (без расширения)
Если вы хотите использовать FoxLang в Zed без сборки расширения, добавьте в `~/.config/zed/settings.json`:
```json
{
  "lsp": {
    "foxlang-lsp": {
      "binary": {
        "path": "foxlang-lsp",
        "arguments": ["--stdio"]
      }
    }
  },
  "languages": {
    "C": {
      "language_servers": ["foxlang-lsp", "..."]
    }
  },
  "file_types": {
    "C": ["fox"]
  }
}
```

### Публикация расширения в официальный реестр Zed
Zed использует открытый репозиторий [zed-industries/extensions](https://github.com/zed-industries/extensions):
1. Опубликуйте каталог `editors/zed` в отдельном публичном репозитории GitHub (например, `https://github.com/SkrinVex/zed-foxlang`).
2. Сделайте Fork репозитория `zed-industries/extensions`.
3. Добавьте submodule:
   ```bash
   git submodule add https://github.com/SkrinVex/zed-foxlang.git extensions/foxlang
   ```
4. Создайте Pull Request в `zed-industries/extensions`. После мерджа расширение станет доступно во вкладке **Extensions** редактора Zed.

---

## 6. Проверка работы и отладка

Вы можете вручную протестировать взаимодействие с `foxlang-lsp` через командную строку:

```bash
# 1. Запуск в интерактивном режиме JSON-RPC:
build/foxlang-lsp

# 2. Пример запроса инициализации (отправляется клиентом):
Content-Length: 64\r\n\r\n{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"capabilities":{}}}
```

Сервер мгновенно вернёт ответ с полным списком возможностей. Все диагностические логи выводятся в `stderr`.

