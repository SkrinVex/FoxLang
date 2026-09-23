# FoxLang для текстового редактора Kate (KDE)

Интеграция языка FoxLang с редактором Kate: подсветка синтаксиса и полная поддержка Language Server Protocol (LSP).

## 1. Подсветка синтаксиса (KSyntaxHighlighting)

Kate использует формат KSyntaxHighlighting XML для распознавания языков и правил раскраски кода.

### Установка подсветки:
Скопируйте `foxlang.xml` в каталог пользовательских синтаксисов KDE:

```bash
mkdir -p ~/.local/share/org.kde.syntax-highlighting/syntax/
cp editors/kate/foxlang.xml ~/.local/share/org.kde.syntax-highlighting/syntax/
```

После перезапуска Kate файлы с расширением `.fox` будут автоматически распознаваться с подсветкой синтаксиса FoxLang (ключевые слова, типы, строки, числа, встроенные функции stdlib).

---

## 2. Подключение Language Server (`foxlang-lsp`) в Kate

В Kate встроен мощный LSP-клиент (плагин **LSP Client** / **Клиент LSP**).

### Шаг 1. Включение плагина LSP Client в Kate
1. Откройте Kate.
2. Перейдите в **Settings** -> **Configure Kate...** -> вкладка **Plugins** (Плагины).
3. Установите галочку напротив **LSP Client** (Клиент LSP) и нажмите **Apply**.

### Шаг 2. Добавление конфигурации `foxlang-lsp`
1. Перейдите в **Settings** -> **Configure Kate...** -> раздел **LSP Client** -> вкладка **User Server Settings** (Пользовательские настройки сервера).
2. Либо отредактируйте файл конфигурации напрямую:
   ```bash
   mkdir -p ~/.config/kate/lspclient
   ```
   Добавьте сервер `foxlang` в `~/.config/kate/lspclient/settings.json`:
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
   *Примечание: Если бинарник `foxlang-lsp` ещё не добавлен в системный `PATH`, укажите абсолютный путь к нему, например: `["/home/user/FoxLang/build/foxlang-lsp"]`.*

3. Нажмите **Apply** / Сохранить.

---

## 3. Доступные возможности в Kate

После настройки при открытии любого `.fox` файла в нижней строке состояния появится значок активного LSP-сервера `foxlang-lsp`:
* **Ошибки и предупреждения (Diagnostics)**: подчёркивание ошибок в реальном времени, всплывающие подсказки и вкладка **Diagnostics** в нижней панели Kate.
* **Автодополнение (Auto-completion)**: список ключевых слов, функций и переменных при наборе.
* **Подсказки при наведении (Hover)**: всплывающее окно с сигнатурой функции или типом переменной.
* **Переход к определению (Go to Definition)**: клавиша `F12` или контекстное меню правой кнопки мыши -> *Go to Definition*.
* **Символы документа**: панель навигации по функциям и переменным файла.
