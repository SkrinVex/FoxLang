# FoxLang для текстового редактора Kate (KDE)

Интеграция языка FoxLang с редактором Kate: подсветка синтаксиса и полная поддержка Language Server Protocol (LSP).

Файлы конфигурации в этом каталоге:
* [`foxlang.xml`](foxlang.xml) — XML-схема подсветки синтаксиса (KSyntaxHighlighting).
* [`settings.json`](settings.json) — готовый фрагмент конфигурации для плагина LSP Client.
* [`externaltools/`](externaltools/) — внешние инструменты «FoxLang: запустить», «проверить» и «собрать приложение».

> **💡 Быстрая установка**: Скрипт `install.sh` из дистрибутива FoxLang автоматически устанавливает подсветку синтаксиса и регистрирует LSP для Kate!

## Запуск, проверка и сборка

Инструменты из [`externaltools/`](externaltools/) появляются в меню **Сервис → Внешние
инструменты → FoxLang**. Они сохраняют все документы, запускают `foxlang` в папке
текущего файла и показывают вывод в нижней панели. `install.sh` устанавливает их сам;
вручную:

```bash
mkdir -p ~/.config/kate/externaltools
cp editors/kate/externaltools/*.ini ~/.config/kate/externaltools/
```

Кнопку запуска можно вынести на панель инструментов (**Настройка → Панели
инструментов**) и назначить ей клавишу (**Настройка → Комбинации клавиш**).

## 1. Подсветка синтаксиса (KSyntaxHighlighting)

Kate использует формат KSyntaxHighlighting XML для распознавания языков и правил раскраски кода.

### Ручная установка подсветки:
Скопируйте [`foxlang.xml`](foxlang.xml) в каталог пользовательских синтаксисов KDE:

```bash
mkdir -p ~/.local/share/org.kde.syntax-highlighting/syntax/
cp editors/kate/foxlang.xml ~/.local/share/org.kde.syntax-highlighting/syntax/
```

Файл также доступен для прямой загрузки со страницы релизов: [`foxlang.xml`](https://github.com/SkrinVex/FoxLang/releases/latest/download/foxlang.xml).

После этого файлы с расширением `.fox` будут автоматически распознаваться с подсветкой синтаксиса FoxLang (ключевые слова, типы, строки, числа, встроенные функции stdlib).

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
 
---

Подробное описание архитектуры и возможностей языкового сервера приведено в документе [`docs/EDITORS.md`](../../docs/EDITORS.md).
