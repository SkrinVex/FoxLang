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
  * Встроенные функции и функции стандартной библиотеки: `print`, `fox`, `input`, `round`, `random`, `json_get`, `http_get`, `server_start`, `listen` и др.
* **Интеграция с Language Server (`foxlang-lsp`)**:
  * **Диагностика ошибок в реальном времени**: лексические, синтаксические и семантические ошибки с точным указанием строки и столбца (UTF-16 code units для корректной поддержки кириллицы и эмодзи).
  * **Автодополнение (Autocomplete)**: ключевые слова, функции stdlib и пользовательские переменные/функции в текущей области видимости.
  * **Информация при наведении (Hover)**: сигнатуры функций и типы переменных в блоках Markdown.
  * **Переход к определению (Go to Definition)**: `F12` или `Ctrl+Click` для перехода к месту объявления.
  * **Символы документа (Document Symbols)**: навигация по структуре файла (`Ctrl+Shift+O`).

## Установка и запуск

### Способ 1. Установка из репозитория (в 1 команду)
1. Скопируйте каталог расширения в директорию расширений VS Code:
   ```bash
   mkdir -p ~/.vscode/extensions
   cp -r editors/vscode ~/.vscode/extensions/foxlang
   ```
2. Установите зависимости клиента LSP:
   ```bash
   cd ~/.vscode/extensions/foxlang && npm install
   ```
3. Перезапустите VS Code или выполните команду `Developer: Reload Window` (`Ctrl+Shift+P`).

### Способ 2. Загрузка готового архива из релиза
Готовый архив расширения доступен на странице релизов:
👉 **[foxlang-vscode.zip](https://github.com/SkrinVex/FoxLang/releases/latest/download/foxlang-vscode.zip)**

Распакуйте его в `~/.vscode/extensions/foxlang`:
```bash
mkdir -p ~/.vscode/extensions/foxlang
curl -L -o /tmp/foxlang-vscode.zip https://github.com/SkrinVex/FoxLang/releases/latest/download/foxlang-vscode.zip
unzip /tmp/foxlang-vscode.zip -d ~/.vscode/extensions/
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
