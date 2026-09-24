# FoxLang для Visual Studio Code

Официальное расширение поддержки языка FoxLang для Visual Studio Code.

## Файлы расширения в этом каталоге
* [`package.json`](package.json) — манифест расширения и регистрация языка `.fox`.
* [`syntaxes/foxlang.tmLanguage.json`](syntaxes/foxlang.tmLanguage.json) — правила TextMate для подсветки синтаксиса.
* [`language-configuration.json`](language-configuration.json) — настройка автозакрытия скобок, кавычек и правил комментариев.
* [`client/extension.js`](client/extension.js) — клиент LSP для запуска и управления сервером `foxlang-lsp`.

## Возможности

* **Распознавание файлов `.fox`**: автоматическое связывание и значок языка.
* **Подсветка синтаксиса (TextMate)**:
  * Ключевые слова: `if`, `else`, `while`, `for`, `switch`, `case`, `default`, `break`, `continue`, `return`, `using`, `include`, `global`.
  * Типы: `int`, `float`, `string`, `bool`, `void`, `array`.
  * Константы: `true`, `false`, числа, строки с экранированием.
  * Встроенные функции и функции стандартной библиотеки (`print`, `input`, `push`, `str_split`, `http_request`, `listen` и др.). Список генерируется из каталога встроенных функций и `std/*.fox` командой `python3 packaging/sync_editor_builtins.py`.
  * Escape-последовательности в строках, включая `\u041b` и `\u{1F98A}`.
* **Интеграция с Language Server (`foxlang-lsp`)**:
  * **Диагностика ошибок в реальном времени**: лексические, синтаксические и семантические ошибки с точным указанием строки и столбца (UTF-16 code units для корректной поддержки кириллицы и эмодзи).
  * **Автодополнение (Autocomplete)**: ключевые слова, функции stdlib и пользовательские переменные/функции в текущей области видимости.
  * **Информация при наведении (Hover)**: сигнатуры функций и типы переменных в блоках Markdown.
  * **Подсказки параметров (Signature Help)**: активный аргумент при вводе `(` и `,`, включая необязательные параметры встроенных функций.
  * **Переход к определению (Go to Definition)**: `F12` или `Ctrl+Click` для перехода к месту объявления.
  * **Символы документа (Document Symbols)**: навигация по структуре файла (`Ctrl+Shift+O`).

## Установка и запуск

Расширение не требует `npm install`: клиент LSP написан без внешних зависимостей.
Отдельно нужен исполняемый файл `foxlang-lsp` — из пакета выпуска или из сборки проекта.

### Способ 1. Пакет VSIX из выпуска
Скачайте [`foxlang.vsix`](https://github.com/SkrinVex/FoxLang/releases/latest/download/foxlang.vsix) и установите:
```bash
code --install-extension foxlang.vsix
```

### Способ 2. Каталог из репозитория
```bash
mkdir -p ~/.vscode/extensions
cp -r editors/vscode ~/.vscode/extensions/SkrinVex.foxlang-language
```
Затем перезапустите VS Code или выполните `Developer: Reload Window` (`Ctrl+Shift+P`).

### Способ 3. Архив из выпуска
[`foxlang-vscode.zip`](https://github.com/SkrinVex/FoxLang/releases/latest/download/foxlang-vscode.zip) содержит тот же каталог:
```bash
curl -L -o foxlang-vscode.zip https://github.com/SkrinVex/FoxLang/releases/latest/download/foxlang-vscode.zip
unzip foxlang-vscode.zip -d ~/.vscode/extensions/
```

### Настройка пути к `foxlang-lsp`
В настройках VS Code (`settings.json`) при необходимости можно явно указать путь к серверу:
```json
{
  "foxlang.lsp.serverPath": "foxlang-lsp"
}
```
По умолчанию расширение проверяет команду `foxlang-lsp` в системном `PATH`, а также путь `./build/foxlang-lsp` в корне открытого проекта.

---

Подробное описание архитектуры языкового сервера приведено в [`docs/EDITORS.md`](../../docs/EDITORS.md).
