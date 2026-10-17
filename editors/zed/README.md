# Skrumi Extension for Zed IDE

Официальное расширение поддержки языка Skrumi для [Zed IDE](https://zed.dev).

## Возможности

* **Распознавание файлов `.skm`**: автоматическое определение типа файла `Skrumi`.
* **Подсветка синтаксиса на базе Tree-sitter**: ключевые слова, типы, встроенные функции стандартной библиотеки, операторы, строки, комментарии.
* **Автозакрытие и подсветка парных скобок**: `{}`, `[]`, `()`, кавычки.
* **Полная интеграция с Skrumi LSP (`skrumi-lsp`)**:
  * Подсветка синтаксических и семантических ошибок на лету (diagnostics);
  * Контекстное автодополнение ключевых слов, функций и переменных;
  * Информация о типах и сигнатурах при наведении курсора (hover);
  * Быстрый переход к определению символов (go to definition).

---

## Запуск программ

Расширение добавляет задачи ([`languages/skrumi/tasks.json`](languages/skrumi/tasks.json)):
«Skrumi: запустить», «Skrumi: проверить» и «Skrumi: собрать» для открытого файла.
Откройте палитру команд и выполните `task: spawn`.

## Отладка

Расширение регистрирует отладчик **Skrumi** на основе `skrumi debug-adapter`
(нужен Skrumi 6.4 или новее в `PATH`). Запуск — `debugger: start`, либо сценарий в
`.zed/debug.json`:

```json
[
  {
    "label": "Skrumi: main.skm",
    "adapter": "Skrumi",
    "request": "launch",
    "program": "$ZED_WORKTREE_ROOT/main.skm",
    "cwd": "$ZED_WORKTREE_ROOT"
  }
]
```

Подробнее — [docs/DEBUGGING.md](../../docs/DEBUGGING.md).

## Вариант 1. Тестирование и установка Dev-расширения локально в Zed

Для того чтобы протестировать или использовать расширение прямо сейчас без публикации:

1. Убедитесь, что `skrumi` и `skrumi-lsp` установлены в вашей системе (например, в `/usr/local/bin`):
   ```bash
   skrumi-lsp --version
   ```
2. Откройте **Zed IDE**.
3. Откройте командную палитру: `Ctrl+Shift+P` (Linux/Windows) или `Cmd+Shift+P` (macOS).
4. Введите команду:
   ```text
   zed: install dev extension
   ```
5. В открывшемся диалоге выбора папки выберите каталог расширения:
   ```text
   /home/skrinvex/projects/Skrumi/editors/zed
   ```
6. Zed автоматически скомпилирует расширение WebAssembly и активирует его!
   Теперь при открытии любого `.skm` файла будет работать подсветка и языковой сервер `skrumi-lsp`.

---

## Вариант 2. Быстрое подключение через `settings.json` (без расширения)

Если вы хотите подключить Skrumi в Zed прямо сейчас через конфигурационный файл пользователя (`~/.config/zed/settings.json`):

```json
{
  "lsp": {
    "skrumi-lsp": {
      "binary": {
        "path": "skrumi-lsp",
        "arguments": ["--stdio"]
      }
    }
  },
  "languages": {
    "C": {
      "language_servers": ["skrumi-lsp", "..."]
    }
  },
  "file_types": {
    "C": ["skm", "skrumi"]
  }
}
```

---

## Как опубликовать расширение в официальный каталог Zed

Каталог расширений Zed является централизованным и открытым для сообщества. Публикация происходит через Pull Request в официальный репозиторий [zed-industries/extensions](https://github.com/zed-industries/extensions).

### Пошаговая инструкция:

1. **Создайте отдельный публичный Git-репозиторий для расширения** (или используйте репозиторий Skrumi):
   * Рекомендуется создать репозиторий на GitHub: `https://github.com/SkrinVex/zed-skrumi`.
   * Скопируйте туда содержимое папки `editors/zed`:
     ```bash
     git clone git@github.com:SkrinVex/zed-skrumi.git
     cp -R /home/skrinvex/projects/Skrumi/editors/zed/* zed-skrumi/
     cd zed-skrumi
     git add .
     git commit -m "feat: initial Skrumi extension for Zed"
     git push origin main
     ```

2. **Сделайте Fork официального репозитория расширений Zed**:
   * Перейдите на [github.com/zed-industries/extensions](https://github.com/zed-industries/extensions) и нажмите кнопку **Fork**.
   * Клонируйте свой форк на локальный компьютер:
     ```bash
     git clone git@github.com:<ваш-юзернейм>/extensions.git
     cd extensions
     git checkout -b add-skrumi
     ```

3. **Добавьте репозиторий расширения как Git Submodule**:
   * В каталоге вашего форка выполните команду (обязательно через HTTPS, не SSH!):
     ```bash
     git submodule add https://github.com/SkrinVex/zed-skrumi.git extensions/skrumi
     ```

4. **Закоммитьте и отправьте изменения**:
   ```bash
   git add .gitmodules extensions/skrumi
   git commit -m "Add Skrumi extension"
   git push origin add-skrumi
   ```

5. **Откройте Pull Request**:
   * Перейдите в [github.com/zed-industries/extensions](https://github.com/zed-industries/extensions).
   * GitHub предложит создать Pull Request из вашей ветки `add-skrumi`.
   * CI-боты Zed автоматически проверят `extension.toml`, скомпилируют WebAssembly-модуль и запустят тесты валидации.
   * После аппрува мейнтейнерами расширение сразу появится во встроенном каталоге расширений Zed (`Extensions` меню) для всех пользователей в мире!
