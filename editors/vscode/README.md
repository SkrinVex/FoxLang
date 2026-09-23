# FoxLang для Visual Studio Code

Официальное расширение поддержки языка FoxLang для Visual Studio Code.

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

### 1. Сборка `foxlang-lsp`
Скомпилируйте FoxLang и языковой сервер:
```bash
cmake -S . -B build
cmake --build build
```
Убедитесь, что бинарник `foxlang-lsp` находится в `build/` или установлен в системный `PATH` (например, `/usr/local/bin`):
```bash
build/foxlang-lsp --version
```

### 2. Подключение расширения в VS Code
Для использования локального расширения:
1. Скопируйте каталог `editors/vscode` в директорию расширений VS Code:
   ```bash
   cp -r editors/vscode ~/.vscode/extensions/foxlang
   ```
2. Перезапустите VS Code или выполните `Developer: Reload Window` (`Ctrl+Shift+P`).
3. При открытии любого `.fox` файла расширение автоматически запустит `foxlang-lsp`.

### 3. Настройка пути к `foxlang-lsp`
В файле `settings.json` VS Code можно явно указать путь к серверу:
```json
{
  "foxlang.lsp.serverPath": "/path/to/FoxLang/build/foxlang-lsp"
}
```
По умолчанию расширение проверяет `build/foxlang-lsp` в текущем открытом каталоге проекта либо запускает команду `foxlang-lsp` из `PATH`.
