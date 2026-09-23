# Тестирование FoxLang

Тестовый набор FoxLang состоит из:
1. **Native C++ unit-тестов** компонентов ядра (`test_lexer`, `test_parser`, `test_interpreter`);
2. **Smoke-тестов** синтаксиса и стандартной библиотеки;
3. **Регрессионных тестов языка** (`tests/regression/*.fox`);
4. **Интеграционных тестов окружения и сети** (уровни логирования, обработка ошибок, локальный HTTP клиент/сервер).

## Запуск через CTest

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Все тесты завершаются с ненулевым статусом при обнаружении регрессий. Тестирование сетевых компонентов выполняется против локального тестового сервера без обращения во внешнюю сеть.

## Standalone

Для интеграционных тестов нужен Python 3; для готовых программ он не требуется.

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
готового executable. Сетевые тесты используют localhost: HTTP-клиент требует curl;
HTTP-сервер и webhook проверяются на Linux/POSIX. CI выполняет остальные сценарии
также на Windows. Hello-тест выводит размер executable в байтах.
