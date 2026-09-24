# FoxLang Extension for Zed IDE

Официальное расширение поддержки языка FoxLang для [Zed IDE](https://zed.dev).

## Возможности

* **Распознавание файлов `.fox`**: автоматическое определение типа файла `FoxLang`.
* **Подсветка синтаксиса на базе Tree-sitter**: ключевые слова, типы, встроенные функции стандартной библиотеки, операторы, строки, комментарии.
* **Автозакрытие и подсветка парных скобок**: `{}`, `[]`, `()`, кавычки.
* **Полная интеграция с FoxLang LSP (`foxlang-lsp`)**:
  * Подсветка синтаксических и семантических ошибок на лету (diagnostics);
  * Контекстное автодополнение ключевых слов, функций и переменных;
  * Информация о типах и сигнатурах при наведении курсора (hover);
  * Быстрый переход к определению символов (go to definition).

---

## Запуск программ

Расширение добавляет задачи ([`languages/foxlang/tasks.json`](languages/foxlang/tasks.json)):
«FoxLang: запустить», «FoxLang: проверить» и «FoxLang: собрать» для открытого файла.
Откройте палитру команд и выполните `task: spawn`.

## Вариант 1. Тестирование и установка Dev-расширения локально в Zed

Для того чтобы протестировать или использовать расширение прямо сейчас без публикации:

1. Убедитесь, что `foxlang` и `foxlang-lsp` установлены в вашей системе (например, в `/usr/local/bin`):
   ```bash
   foxlang-lsp --version
   ```
2. Откройте **Zed IDE**.
3. Откройте командную палитру: `Ctrl+Shift+P` (Linux/Windows) или `Cmd+Shift+P` (macOS).
4. Введите команду:
   ```text
   zed: install dev extension
   ```
5. В открывшемся диалоге выбора папки выберите каталог расширения:
   ```text
   /home/skrinvex/projects/FoxLang/editors/zed
   ```
6. Zed автоматически скомпилирует расширение WebAssembly и активирует его!
   Теперь при открытии любого `.fox` файла будет работать подсветка и языковой сервер `foxlang-lsp`.

---

## Вариант 2. Быстрое подключение через `settings.json` (без расширения)

Если вы хотите подключить FoxLang в Zed прямо сейчас через конфигурационный файл пользователя (`~/.config/zed/settings.json`):

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

---

## Как опубликовать расширение в официальный каталог Zed

Каталог расширений Zed является централизованным и открытым для сообщества. Публикация происходит через Pull Request в официальный репозиторий [zed-industries/extensions](https://github.com/zed-industries/extensions).

### Пошаговая инструкция:

1. **Создайте отдельный публичный Git-репозиторий для расширения** (или используйте репозиторий FoxLang):
   * Рекомендуется создать репозиторий на GitHub: `https://github.com/SkrinVex/zed-foxlang`.
   * Скопируйте туда содержимое папки `editors/zed`:
     ```bash
     git clone git@github.com:SkrinVex/zed-foxlang.git
     cp -R /home/skrinvex/projects/FoxLang/editors/zed/* zed-foxlang/
     cd zed-foxlang
     git add .
     git commit -m "feat: initial FoxLang extension for Zed"
     git push origin main
     ```

2. **Сделайте Fork официального репозитория расширений Zed**:
   * Перейдите на [github.com/zed-industries/extensions](https://github.com/zed-industries/extensions) и нажмите кнопку **Fork**.
   * Клонируйте свой форк на локальный компьютер:
     ```bash
     git clone git@github.com:<ваш-юзернейм>/extensions.git
     cd extensions
     git checkout -b add-foxlang
     ```

3. **Добавьте репозиторий расширения как Git Submodule**:
   * В каталоге вашего форка выполните команду (обязательно через HTTPS, не SSH!):
     ```bash
     git submodule add https://github.com/SkrinVex/zed-foxlang.git extensions/foxlang
     ```

4. **Закоммитьте и отправьте изменения**:
   ```bash
   git add .gitmodules extensions/foxlang
   git commit -m "Add FoxLang extension"
   git push origin add-foxlang
   ```

5. **Откройте Pull Request**:
   * Перейдите в [github.com/zed-industries/extensions](https://github.com/zed-industries/extensions).
   * GitHub предложит создать Pull Request из вашей ветки `add-foxlang`.
   * CI-боты Zed автоматически проверят `extension.toml`, скомпилируют WebAssembly-модуль и запустят тесты валидации.
   * После аппрува мейнтейнерами расширение сразу появится во встроенном каталоге расширений Zed (`Extensions` меню) для всех пользователей в мире!
