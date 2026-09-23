#include "foxlang/SemanticAnalyzer.h"
#include "foxlang/Lexer.h"
#include "foxlang/Parser.h"
#include "foxlang/Runtime.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_set>

namespace foxlang {

SemanticAnalyzer::SemanticAnalyzer(std::string curFile, std::string home)
    : currentFile(std::move(curFile)), foxHome(std::move(home)) {
    rootScope = std::make_unique<Scope>();
    currentScope = rootScope.get();
    addBuiltins();
}

SemanticAnalyzer::~SemanticAnalyzer() = default;

void SemanticAnalyzer::enterScope() {
    auto newScope = std::make_unique<Scope>();
    newScope->parent = currentScope;
    currentScope = newScope.release();
}

void SemanticAnalyzer::exitScope() {
    if (currentScope && currentScope->parent) {
        Scope* parent = currentScope->parent;
        delete currentScope;
        currentScope = parent;
    }
}

void SemanticAnalyzer::addBuiltins() {
    auto addFn = [this](std::string name, std::string ret, std::vector<FuncParam> params, std::string doc) {
        Symbol sym;
        sym.name = name;
        sym.type = ret;
        sym.kind = SymbolKind::Builtin;
        sym.returnType = ret;
        sym.params = std::move(params);
        sym.documentation = std::move(doc);
        rootScope->symbols[name] = sym;
    };

    addFn("print", "void", {},
        "Вывод значений в стандартный поток вывода с переводом строки.\n\n"
        "**Параметры:**\n- `...args`: аргументы любого типа для печати\n\n"
        "**Пример:**\n```foxlang\nprint(\"Привет, мир! 🦊\");\n```");

    addFn("input", "string", {},
        "Чтение строки из стандартного потока ввода пользователя.\n\n"
        "**Возвращает:** `string` — введённая строка\n\n"
        "**Пример:**\n```foxlang\nstring name = input();\n```");

    addFn("fox", "void", {},
        "Печать фирменного ASCII-баннера лисы FoxLang в консоль.\n\n"
        "**Пример:**\n```foxlang\nfox();\n```");

    addFn("round", "int", {{"float", "value"}},
        "Округление числа с плавающей точкой до ближайшего целого.\n\n"
        "**Параметры:**\n- `value`: дробное число `float`\n\n"
        "**Возвращает:** `int`\n\n"
        "**Пример:**\n```foxlang\nint r = round(3.6); // 4\n```");

    addFn("random", "int", {{"int", "min"}, {"int", "max"}},
        "Генерация псевдослучайного целого числа в диапазоне от `min` до `max` включительно.\n\n"
        "**Параметры:**\n- `min`: минимальное значение диапазона\n- `max`: максимальное значение диапазона\n\n"
        "**Возвращает:** `int`\n\n"
        "**Пример:**\n```foxlang\nint dice = random(1, 6);\n```");

    addFn("read_file", "string", {{"string", "path"}},
        "Чтение всего содержимого файла в виде строки.\n\n"
        "**Параметры:**\n- `path`: путь к файлу на диске\n\n"
        "**Возвращает:** `string` (пустая строка, если файл не найден)\n\n"
        "**Пример:**\n```foxlang\nstring content = read_file(\"config.json\");\n```");

    addFn("readfile", "string", {{"string", "path"}},
        "Псевдоним для `read_file`: чтение всего содержимого файла в виде строки.");

    addFn("write_file", "void", {{"string", "path"}, {"string", "data"}},
        "Запись строковых данных в файл (перезапись содержимого файла).\n\n"
        "**Параметры:**\n- `path`: путь к файлу\n- `data`: записываемый текст");

    addFn("append_file", "void", {{"string", "path"}, {"string", "data"}},
        "Добавление строковых данных в конец файла.\n\n"
        "**Параметры:**\n- `path`: путь к файлу\n- `data`: добавляемый текст");

    addFn("json_get", "string", {{"string", "json"}, {"string", "key"}},
        "Извлечение строкового значения из JSON-строки по ключу.\n\n"
        "**Параметры:**\n- `json`: JSON-строка\n- `key`: имя ключа верхнего уровня");

    addFn("json_escape", "string", {{"string", "str"}},
        "Экранирование спецсимволов строки для безопасного включения в JSON.");

    addFn("str_contains", "bool", {{"string", "str"}, {"string", "sub"}},
        "Проверка вхождения подстроки `sub` в строку `str`.\n\n"
        "**Возвращает:** `bool` (`true` если подстрока найдена)");

    addFn("str_replace", "string", {{"string", "str"}, {"string", "from"}, {"string", "to"}},
        "Замена всех вхождений подстроки `from` на `to` в строке `str`.");

    addFn("str_split", "void", {{"string", "str"}, {"string", "delim"}},
        "Разбиение строки по разделителю `delim`.");

    addFn("str_to_int", "int", {{"string", "str"}},
        "Преобразование строки в целое число `int`.\n\n"
        "**Параметры:**\n- `str`: строка, содержащая десятичное число\n\n"
        "**Возвращает:** `int`\n\n"
        "**Пример:**\n```foxlang\nint val = str_to_int(\"42\");\n```");

    addFn("env_get", "string", {{"string", "key"}},
        "Чтение переменной окружения или файла конфигурации `.env`.\n\n"
        "**Возвращает:** `string` (пустая строка, если переменная не установлена)");

    addFn("env_required", "string", {{"string", "key"}},
        "Чтение обязательной переменной окружения или секрета.\n"
        "Если переменная не установлена, рантайм вызывает ошибку `Environment Error`.\n\n"
        "**Пример:**\n```foxlang\nstring tok = env_required(\"API_KEY\");\n```");

    addFn("env_default", "string", {{"string", "key"}, {"string", "fallback"}},
        "Чтение переменной окружения с возвратом значения по умолчанию `fallback`.\n\n"
        "**Параметры:**\n- `key`: имя переменной\n- `fallback`: значение по умолчанию\n\n"
        "**Возвращает:** `string`\n\n"
        "**Пример:**\n```foxlang\nstring port = env_default(\"PORT\", \"8080\");\n```");

    addFn("getch", "string", {},
        "Неблокирующее чтение одного символа с клавиатуры без вывода в консоль (эхо).\n\n"
        "**Возвращает:** `string` — прочитанный символ");

    addFn("kbhit", "bool", {},
        "Проверка наличия нажатой клавиши в буфере ввода терминала.\n\n"
        "**Возвращает:** `bool` (`true`, если клавиша была нажата)");

    addFn("wait", "void", {{"int", "milliseconds"}},
        "Приостановка выполнения текущей программы на указанное число миллисекунд.");

    addFn("httpget", "string", {{"string", "url"}},
        "Низкоуровневый исходящий HTTP GET-запрос по URL.");

    addFn("httppost", "string", {{"string", "url"}, {"string", "data"}},
        "Низкоуровневый исходящий HTTP POST-запрос по URL.");

    addFn("httpput", "string", {{"string", "url"}, {"string", "data"}},
        "Низкоуровневый исходящий HTTP PUT-запрос.");

    addFn("httpdelete", "string", {{"string", "url"}},
        "Низкоуровневый исходящий HTTP DELETE-запрос.");

    addFn("server_start", "string", {{"int", "port"}},
        "Низкоуровневый запуск встроенного HTTP-сервера на порту.");

    addFn("server_stop", "string", {},
        "Остановка запущенного встроенного HTTP-сервера.");

    addFn("route_get", "string", {{"string", "path"}, {"string", "handler"}},
        "Регистрация маршрута для входящих HTTP GET-запросов.");

    addFn("route_post", "string", {{"string", "path"}, {"string", "handler"}},
        "Регистрация маршрута для входящих HTTP POST-запросов.");

    addFn("send_response", "void", {{"string", "data"}},
        "Отправка тела ответа клиенту HTTP-сервера.");

    addFn("array", "void", {{"identifier", "name"}, {"int", "size"}},
        "Синтаксис объявления массива фиксированного размера: `array <имя> <размер>;`.");

    addFn("set", "void", {{"array", "arr"}, {"int", "idx"}, {"any", "val"}},
        "Установка значения элемента массива по индексу.\n\n"
        "**Параметры:**\n- `arr`: массив\n- `idx`: индекс (начиная с 0)\n- `val`: записываемое значение\n\n"
        "**Пример:**\n```foxlang\nset(numbers, 0, 100);\n```");

    addFn("get", "any", {{"array", "arr"}, {"int", "idx"}},
        "Получение элемента массива по индексу или регистрация HTTP GET маршрута при импорте `using server;`.\n\n"
        "**Параметры:**\n- `arr`: массив\n- `idx`: индекс (начиная с 0)\n\n"
        "**Пример:**\n```foxlang\nint val = get(numbers, 0);\n```");

    addFn("size", "int", {{"any", "val"}},
        "Получение количества элементов в массиве или длины строки в символах.\n\n"
        "**Параметры:**\n- `val`: массив или строка\n\n"
        "**Возвращает:** `int` — размер\n\n"
        "**Пример:**\n```foxlang\nint n = size(my_arr);\n```");
}

void SemanticAnalyzer::loadModuleSymbols(const std::string& moduleName, SourceRange importRange) {
    auto addFn = [this](std::string name, std::string ret, std::vector<FuncParam> params, std::string doc) {
        Symbol sym;
        sym.name = name;
        sym.type = ret;
        sym.kind = SymbolKind::Function;
        sym.returnType = ret;
        sym.params = std::move(params);
        sym.documentation = std::move(doc);
        rootScope->symbols[name] = sym;
    };

    if (moduleName == "server") {
        addFn("listen", "void", {{"int", "port"}},
            "Запустить HTTP/webhook сервер на указанном порту (0.0.0.0).\n"
            "Блокирует текущий поток до вызова `server_stop()`.\n\n"
            "**Параметры:**\n- `port`: номер TCP-порта (например, `8080`)\n\n"
            "**Пример:**\n```foxlang\nlisten(8080);\n```");

        addFn("get", "void", {{"string", "path"}, {"string", "handler"}},
            "Зарегистрировать обработчик входящих HTTP GET запросов по пути `path`.\n\n"
            "**Параметры:**\n- `path`: URL-путь (например, `\"/health\"`)\n- `handler`: имя функции-обработчика\n\n"
            "**Пример:**\n```foxlang\nget(\"/health\", \"health\");\n```");

        addFn("post", "void", {{"string", "path"}, {"string", "handler"}},
            "Зарегистрировать обработчик входящих HTTP POST запросов по пути `path`.\n\n"
            "**Параметры:**\n- `path`: URL-путь (например, `\"/telegram\"`)\n- `handler`: имя функции-обработчика\n\n"
            "**Пример:**\n```foxlang\npost(\"/telegram\", \"telegram_webhook\");\n```");

        addFn("body", "string", {},
            "Получить тело текущего входящего HTTP-запроса (JSON update, форма, текст).\n\n"
            "**Возвращает:** `string`\n\n"
            "**Пример:**\n```foxlang\nstring update = body();\n```");

        addFn("method", "string", {},
            "Получить метод входящего HTTP-запроса (`\"GET\"`, `\"POST\"`).\n\n"
            "**Возвращает:** `string`");

        addFn("path", "string", {},
            "Получить запрошенный URL-путь (например, `\"/telegram\"`).\n\n"
            "**Возвращает:** `string`");

        addFn("respond", "void", {{"string", "data"}},
            "Отправить клиенту HTTP-ответ с кодом 200 OK и телом `data`.\n\n"
            "**Параметры:**\n- `data`: тело ответа (обычно JSON)\n\n"
            "**Пример:**\n```foxlang\nrespond(\"{\\\"ok\\\":true}\");\n```");

        addFn("respond_status", "void", {{"int", "status"}, {"string", "data"}},
            "Отправить клиенту HTTP-ответ с заданным кодом статуса `status` и телом `data`.\n\n"
            "**Параметры:**\n- `status`: код состояния HTTP (`200`, `400`, `404`, `500`)\n- `data`: тело ответа\n\n"
            "**Пример:**\n```foxlang\nrespond_status(404, \"{\\\"error\\\":\\\"Not found\\\"}\");\n```");

    } else if (moduleName == "http") {
        addFn("http_fetch", "string", {{"string", "url"}},
            "Выполнить исходящий HTTP GET-запрос по указанному URL.\n\n"
            "**Параметры:**\n- `url`: целевой URL (`\"https://...\"`)\n\n"
            "**Возвращает:** `string` — тело ответа\n\n"
            "**Пример:**\n```foxlang\nstring html = http_fetch(\"https://example.com\");\n```");

        addFn("http_post_json", "string", {{"string", "url"}, {"string", "body"}},
            "Выполнить исходящий HTTP POST-запрос с `Content-Type: application/json`.\n\n"
            "**Параметры:**\n- `url`: целевой URL API\n- `body`: тело запроса в формате JSON\n\n"
            "**Возвращает:** `string` — ответ удалённого сервера\n\n"
            "**Пример:**\n```foxlang\nstring res = http_post_json(api + \"sendMessage\", payload);\n```");

        addFn("http_post_as", "string", {{"string", "url"}, {"string", "body"}, {"string", "content_type"}},
            "Выполнить исходящий HTTP POST-запрос с произвольным заголовком `Content-Type`.\n\n"
            "**Параметры:**\n- `url`: адрес назначения\n- `body`: данные тела запроса\n- `content_type`: MIME-тип контента\n\n"
            "**Возвращает:** `string` — ответ сервера");

        addFn("http_put_json", "string", {{"string", "url"}, {"string", "body"}},
            "Выполнить исходящий HTTP PUT-запрос с `Content-Type: application/json`.\n\n"
            "**Параметры:**\n- `url`: адрес назначения\n- `body`: тело запроса\n\n"
            "**Возвращает:** `string`");

        addFn("http_remove", "string", {{"string", "url"}},
            "Выполнить исходящий HTTP DELETE-запрос по указанному URL.\n\n"
            "**Параметры:**\n- `url`: адрес ресурса\n\n"
            "**Возвращает:** `string`");

    } else if (moduleName == "env") {
        addFn("env", "string", {{"string", "name"}},
            "Получить значение переменной окружения или настройки из файла `.env`.\n\n"
            "**Параметры:**\n- `name`: имя переменной\n\n"
            "**Возвращает:** `string` (пустая строка, если переменная не найдена)\n\n"
            "**Пример:**\n```foxlang\nstring p = env(\"PORT\");\n```");

        addFn("secret", "string", {{"string", "name"}},
            "Получить обязательную переменную окружения или секрет из `.env`.\n"
            "Если переменная не найдена, рантайм аварийно завершает программу с ошибкой.\n\n"
            "**Параметры:**\n- `name`: имя секрета\n\n"
            "**Возвращает:** `string`\n\n"
            "**Пример:**\n```foxlang\nstring token = secret(\"TELEGRAM_BOT_TOKEN\");\n```");

        addFn("env_default", "string", {{"string", "name"}, {"string", "fallback"}},
            "Получить значение переменной окружения `name` или вернуть `fallback`, если переменная не задана.\n\n"
            "**Параметры:**\n- `name`: имя переменной\n- `fallback`: значение по умолчанию\n\n"
            "**Возвращает:** `string`\n\n"
            "**Пример:**\n```foxlang\nstring port = env_default(\"PORT\", \"8080\");\n```");

    } else if (moduleName == "log") {
        addFn("debug", "void", {{"string", "message"}},
            "Записать сообщение уровня `[DEBUG]` (серый цвет в консоли).\n\n"
            "**Параметры:**\n- `message`: текст сообщения");

        addFn("info", "void", {{"string", "message"}},
            "Записать информационное сообщение `[INFO]`.\n\n"
            "**Параметры:**\n- `message`: текст сообщения\n\n"
            "**Пример:**\n```foxlang\ninfo(\"🦊 FoxBot запущен на порту \" + port);\n```");

        addFn("warn", "void", {{"string", "message"}},
            "Записать предупреждающее сообщение `[WARN]` (жёлтый цвет в консоли).\n\n"
            "**Параметры:**\n- `message`: текст сообщения");

        addFn("error", "void", {{"string", "message"}},
            "Записать сообщение об ошибке `[ERROR]` (красный цвет в консоли).\n\n"
            "**Параметры:**\n- `message`: текст сообщения");

    } else if (moduleName == "json") {
        addFn("json_path", "string", {{"string", "json"}, {"string", "path"}},
            "Извлечь строковое значение из JSON по вложенному точечному пути.\n"
            "Корректно декодирует суррогатные пары UTF-16 и Unicode эмодзи (🦊).\n\n"
            "**Параметры:**\n- `json`: исходный текст в формате JSON\n- `path`: путь к полю (например, `\"message.chat.id\"`)\n\n"
            "**Возвращает:** `string`\n\n"
            "**Пример:**\n```foxlang\nstring chat_id = json_path(update, \"message.chat.id\");\n```");

        addFn("json_safe", "string", {{"string", "text"}},
            "Экранировать спецсимволы строки (кавычки `\"`, переносы `\\n`, табы) для безопасной вставки в JSON.\n\n"
            "**Параметры:**\n- `text`: исходный текст\n\n"
            "**Возвращает:** `string`\n\n"
            "**Пример:**\n```foxlang\nstring payload = \"{\\\"text\\\":\\\"\" + json_safe(msg) + \"\\\"}\";\n```");

    } else if (moduleName == "string") {
        addFn("contains", "bool", {{"string", "text"}, {"string", "needle"}},
            "Проверить, содержит ли строка `text` подстроку `needle`.\n\n"
            "**Параметры:**\n- `text`: проверяемая строка\n- `needle`: искомая подстрока\n\n"
            "**Возвращает:** `bool` (`true` или `false`)\n\n"
            "**Пример:**\n```foxlang\nif (contains(msg, \"/start\")) { ... }\n```");

        addFn("replace", "string", {{"string", "text"}, {"string", "from"}, {"string", "to"}},
            "Заменить все вхождения подстроки `from` на `to` в строке `text`.\n\n"
            "**Параметры:**\n- `text`: исходный текст\n- `from`: замещаемый фрагмент\n- `to`: новый фрагмент\n\n"
            "**Возвращает:** `string`\n\n"
            "**Пример:**\n```foxlang\nstring clean = replace(input, \"foo\", \"bar\");\n```");

        addFn("to_int", "int", {{"string", "text"}},
            "Преобразовать строковое представление числа в тип `int`.\n\n"
            "**Параметры:**\n- `text`: строка с числом (например, `\"8080\"`)\n\n"
            "**Возвращает:** `int`\n\n"
            "**Пример:**\n```foxlang\nint port = to_int(port_string);\n```");

        addFn("strtoint", "int", {{"string", "text"}},
            "Псевдоним для `to_int`: преобразовать строку в целое число `int`.\n\n"
            "**Параметры:**\n- `text`: строка с числом\n\n"
            "**Возвращает:** `int`\n\n"
            "**Пример:**\n```foxlang\nint port = strtoint(port_string);\n```");

    } else if (moduleName == "math") {
        addFn("clamp01", "float", {{"float", "value"}},
            "Ограничить дробное число диапазоном от 0.0 до 1.0 включительно.\n\n"
            "**Параметры:**\n- `value`: исходное число `float`\n\n"
            "**Возвращает:** `float`");

        addFn("min_int", "int", {{"int", "a"}, {"int", "b"}},
            "Вычислить минимальное из двух целых чисел.\n\n"
            "**Параметры:**\n- `a`: первое число\n- `b`: второе число\n\n"
            "**Возвращает:** `int`\n\n"
            "**Пример:**\n```foxlang\nint m = min_int(10, 20); // 10\n```");

        addFn("max_int", "int", {{"int", "a"}, {"int", "b"}},
            "Вычислить максимальное из двух целых чисел.\n\n"
            "**Параметры:**\n- `a`: первое число\n- `b`: второе число\n\n"
            "**Возвращает:** `int`\n\n"
            "**Пример:**\n```foxlang\nint m = max_int(10, 20); // 20\n```");

    } else if (moduleName == "net") {
        addFn("connect_tcp", "int", {{"string", "host"}, {"int", "port"}},
            "Установить исходящее TCP-соединение с хостом по порту.\n\n"
            "**Параметры:**\n- `host`: имя хоста или IP-адрес\n- `port`: TCP-порт\n\n"
            "**Возвращает:** `int` — дескриптор сокета (или `-1` при ошибке соединения)");

        addFn("send_tcp", "int", {{"int", "socket"}, {"string", "data"}},
            "Отправить строковые данные в открытый TCP-сокет.\n\n"
            "**Параметры:**\n- `socket`: дескриптор открытого сокета\n- `data`: отправляемые байты\n\n"
            "**Возвращает:** `int` — число отправленных байт");

        addFn("recv_tcp", "string", {{"int", "socket"}, {"int", "max_bytes"}},
            "Прочитать до `max_bytes` байт из открытого TCP-сокета.\n\n"
            "**Параметры:**\n- `socket`: дескриптор сокета\n- `max_bytes`: лимит байт для чтения\n\n"
            "**Возвращает:** `string` — прочитанные данные");

        addFn("close_tcp", "bool", {{"int", "socket"}},
            "Закрыть дескриптор TCP-сокета.\n\n"
            "**Параметры:**\n- `socket`: дескриптор сокета\n\n"
            "**Возвращает:** `bool` (`true` при успешном закрытии)");

        addFn("resolve_host", "string", {{"string", "host"}},
            "Разрешить сетевое доменное имя в IPv4-адрес через DNS.\n\n"
            "**Параметры:**\n- `host`: имя хоста (например, `\"api.telegram.org\"`)\n\n"
            "**Возвращает:** `string` — IP-адрес");

    } else if (moduleName == "terminal") {
        addFn("clear", "void", {},
            "Очистить экран терминала ANSI ESC-последовательностью.\n\n"
            "**Пример:**\n```foxlang\nclear();\n```");

        addFn("home", "void", {},
            "Переместить курсор терминала в верхний левый угол (1, 1).");

        addFn("write", "void", {{"string", "text"}},
            "Вывести текст в терминал напрямую без добавления символа переноса строки.");

        addFn("goto_xy", "void", {{"int", "row"}, {"int", "col"}},
            "Переместить курсор терминала в указанную позицию (строка, колонка).\n\n"
            "**Параметры:**\n- `row`: номер строки (начиная с 1)\n- `col`: номер колонки (начиная с 1)");

        addFn("hide_cursor", "void", {},
            "Скрыть курсор в окне терминала.");

        addFn("show_cursor", "void", {},
            "Показать курсор в окне терминала.");

        addFn("color", "void", {{"int", "ansi_code"}},
            "Установить ANSI-цвет для последующего вывода в терминал.\n\n"
            "**Параметры:**\n- `ansi_code`: числовой ANSI-код (например, 31 - красный, 32 - зелёный)");

        addFn("reset_color", "void", {},
            "Сбросить цвета и текстовые атрибуты оформления терминала к стандартным.");

    } else if (moduleName == "time") {
        addFn("sleep_ms", "void", {{"int", "milliseconds"}},
            "Приостановить выполнение программы на указанное число миллисекунд.\n\n"
            "**Параметры:**\n- `milliseconds`: время задержки в миллисекундах\n\n"
            "**Пример:**\n```foxlang\nsleep_ms(1000); // пауза 1 секунда\n```");

        addFn("unix_time_ms", "string", {},
            "Получить текущее UNIX-время в миллисекундах с 1 января 1970 года.\n\n"
            "**Возвращает:** `string` — таймстемп в миллисекундах\n\n"
            "**Пример:**\n```foxlang\nstring now = unix_time_ms();\n```");
    } else {
        // Attempt to resolve file on disk
        std::string modPath = moduleName;
        if (modPath.size() < 4 || modPath.substr(modPath.size() - 4) != ".fox") {
            modPath += ".fox";
        }
        std::string fullPath;
        try {
            fullPath = runtime::resolveFoxFile("std/" + modPath, currentFile, foxHome);
        } catch (...) {
            try {
                fullPath = runtime::resolveFoxFile(modPath, currentFile, foxHome);
            } catch (...) {
                diagnostics.push_back({DiagnosticSeverity::Warning, "Module '" + moduleName + "' not found", importRange});
                return;
            }
        }

        std::ifstream file(fullPath);
        if (file.is_open()) {
            std::stringstream buf;
            buf << file.rdbuf();
            try {
                Lexer modLexer(buf.str(), true);
                auto modTokens = modLexer.tokenize();
                Parser modParser(std::move(modTokens), fullPath);
                std::vector<Diagnostic> modDiags;
                auto modProg = modParser.parseProgramWithDiagnostics(modDiags);
                for (const auto& stmt : modProg->stmts) {
                    if (auto* fn = dynamic_cast<const FuncDefNode*>(stmt.get())) {
                        Symbol s;
                        s.name = fn->name;
                        s.type = fn->returnType;
                        s.kind = SymbolKind::Function;
                        s.returnType = fn->returnType;
                        s.params = fn->params;
                        s.declRange = fn->range;
                        s.fileUri = fullPath;
                        rootScope->symbols[fn->name] = s;
                    }
                }
            } catch (...) {}
        }
    }
}

void SemanticAnalyzer::analyze(const BlockNode* root) {
    diagnostics.clear();
    symbolRefs.clear();
    documentSymbols.clear();

    if (!root) return;
    visitBlock(root);
}

void SemanticAnalyzer::visitNode(const Node* node) {
    if (!node) return;

    if (auto* blk = dynamic_cast<const BlockNode*>(node)) {
        visitBlock(blk);
    } else if (auto* fn = dynamic_cast<const FuncDefNode*>(node)) {
        visitFuncDef(fn);
    } else if (auto* vd = dynamic_cast<const VarDeclNode*>(node)) {
        visitVarDecl(vd);
    } else if (auto* gvd = dynamic_cast<const GlobalVarDeclNode*>(node)) {
        visitGlobalVarDecl(gvd);
    } else if (auto* va = dynamic_cast<const VarAssignNode*>(node)) {
        visitVarAssign(va);
    } else if (auto* fc = dynamic_cast<const FuncCallNode*>(node)) {
        visitFuncCall(fc);
    } else if (auto* acc = dynamic_cast<const VarAccessNode*>(node)) {
        visitVarAccess(acc);
    } else if (auto* ret = dynamic_cast<const ReturnNode*>(node)) {
        visitReturn(ret);
    } else if (auto* ifn = dynamic_cast<const IfNode*>(node)) {
        visitIf(ifn);
    } else if (auto* wh = dynamic_cast<const WhileNode*>(node)) {
        visitWhile(wh);
    } else if (auto* fr = dynamic_cast<const ForNode*>(node)) {
        visitFor(fr);
    } else if (auto* sw = dynamic_cast<const SwitchNode*>(node)) {
        visitSwitch(sw);
    } else if (auto* bop = dynamic_cast<const BinOpNode*>(node)) {
        visitBinOp(bop);
    } else if (auto* arr = dynamic_cast<const ArrayDeclNode*>(node)) {
        visitArrayDecl(arr);
    } else if (auto* usg = dynamic_cast<const UsingNode*>(node)) {
        visitUsing(usg);
    } else if (auto* inc = dynamic_cast<const IncludeNode*>(node)) {
        visitInclude(inc);
    }
}

void SemanticAnalyzer::visitBlock(const BlockNode* node) {
    for (const auto& stmt : node->stmts) {
        visitNode(stmt.get());
    }
}

void SemanticAnalyzer::visitFuncDef(const FuncDefNode* node) {
    Symbol fnSym;
    fnSym.name = node->name;
    fnSym.type = node->returnType;
    fnSym.kind = SymbolKind::Function;
    fnSym.returnType = node->returnType;
    fnSym.params = node->params;
    fnSym.declRange = node->nameRange.start.line > 0 ? node->nameRange : node->range;
    fnSym.fileUri = currentFile;

    std::string sig = node->returnType + " " + node->name + "(";
    for (size_t i = 0; i < node->params.size(); i++) {
        if (i > 0) sig += ", ";
        sig += node->params[i].type + " " + node->params[i].name;
    }
    sig += ")";
    fnSym.documentation = sig;

    rootScope->symbols[node->name] = fnSym;
    symbolRefs.push_back({fnSym.declRange, fnSym});

    DocumentSymbolInfo docSym;
    docSym.name = node->name;
    docSym.kind = "Function";
    docSym.range = node->range;
    docSym.selectionRange = fnSym.declRange;
    documentSymbols.push_back(docSym);

    enterScope();
    std::string oldReturn = currentFuncReturnType;
    currentFuncReturnType = node->returnType;

    for (const auto& param : node->params) {
        Symbol paramSym;
        paramSym.name = param.name;
        paramSym.type = param.type;
        paramSym.kind = SymbolKind::Parameter;
        paramSym.documentation = "parameter " + param.type + " " + param.name;
        paramSym.fileUri = currentFile;
        paramSym.declRange = node->range; // Encompassed in function signature
        currentScope->symbols[param.name] = paramSym;
    }

    if (node->body) {
        visitNode(node->body.get());
    }

    currentFuncReturnType = oldReturn;
    exitScope();
}

void SemanticAnalyzer::visitVarDecl(const VarDeclNode* node) {
    if (node->expr) {
        visitNode(node->expr.get());
    }

    if (currentScope->findCurrent(node->name)) {
        diagnostics.push_back({DiagnosticSeverity::Warning,
            "Redeclaration of variable '" + node->name + "' in the same scope",
            node->nameRange.start.line > 0 ? node->nameRange : node->range});
    }

    Symbol sym;
    sym.name = node->name;
    sym.type = node->type;
    sym.kind = (currentScope == rootScope.get()) ? SymbolKind::Variable : SymbolKind::Variable;
    sym.declRange = node->nameRange.start.line > 0 ? node->nameRange : node->range;
    sym.documentation = node->type + " " + node->name;
    sym.fileUri = currentFile;

    currentScope->symbols[node->name] = sym;
    symbolRefs.push_back({sym.declRange, sym});

    if (currentScope == rootScope.get()) {
        DocumentSymbolInfo docSym;
        docSym.name = node->name;
        docSym.kind = "Variable";
        docSym.range = node->range;
        docSym.selectionRange = sym.declRange;
        documentSymbols.push_back(docSym);
    }
}

void SemanticAnalyzer::visitGlobalVarDecl(const GlobalVarDeclNode* node) {
    if (node->expr) {
        visitNode(node->expr.get());
    }

    Symbol sym;
    sym.name = node->name;
    sym.type = node->type;
    sym.kind = SymbolKind::Variable;
    sym.declRange = node->nameRange.start.line > 0 ? node->nameRange : node->range;
    sym.documentation = "global " + node->type + " " + node->name;
    sym.fileUri = currentFile;

    rootScope->symbols[node->name] = sym;
    symbolRefs.push_back({sym.declRange, sym});

    DocumentSymbolInfo docSym;
    docSym.name = node->name;
    docSym.kind = "Variable";
    docSym.range = node->range;
    docSym.selectionRange = sym.declRange;
    documentSymbols.push_back(docSym);
}

void SemanticAnalyzer::visitVarAssign(const VarAssignNode* node) {
    if (node->expr) {
        visitNode(node->expr.get());
    }

    Symbol* sym = currentScope->find(node->name);
    SourceRange targetRange = node->nameRange.start.line > 0 ? node->nameRange : node->range;
    if (!sym) {
        diagnostics.push_back({DiagnosticSeverity::Error, "Undefined variable '" + node->name + "'", targetRange});
    } else {
        symbolRefs.push_back({targetRange, *sym});
    }
}

void SemanticAnalyzer::visitVarAccess(const VarAccessNode* node) {
    Symbol* sym = currentScope->find(node->name);
    SourceRange targetRange = node->nameRange.start.line > 0 ? node->nameRange : node->range;
    if (!sym) {
        diagnostics.push_back({DiagnosticSeverity::Error, "Undefined variable '" + node->name + "'", targetRange});
    } else {
        symbolRefs.push_back({targetRange, *sym});
    }
}

void SemanticAnalyzer::visitFuncCall(const FuncCallNode* node) {
    for (const auto& arg : node->args) {
        visitNode(arg.get());
    }

    Symbol* sym = currentScope->find(node->name);
    SourceRange targetRange = node->nameRange.start.line > 0 ? node->nameRange : node->range;

    if (!sym) {
        diagnostics.push_back({DiagnosticSeverity::Error, "Undefined function '" + node->name + "'", targetRange});
        return;
    }

    symbolRefs.push_back({targetRange, *sym});

    // Argument count check for non-varargs
    if (sym->kind == SymbolKind::Function && !sym->params.empty()) {
        if (node->args.size() != sym->params.size()) {
            diagnostics.push_back({DiagnosticSeverity::Error,
                "Function '" + node->name + "' expects " + std::to_string(sym->params.size()) +
                " arguments, but got " + std::to_string(node->args.size()),
                node->range});
        }
    }
}

void SemanticAnalyzer::visitReturn(const ReturnNode* node) {
    if (node->expr) {
        visitNode(node->expr.get());
    }

    if (currentFuncReturnType == "void" && node->expr != nullptr) {
        diagnostics.push_back({DiagnosticSeverity::Error, "Void function should not return a value", node->range});
    }
}

void SemanticAnalyzer::visitIf(const IfNode* node) {
    if (node->condition) visitNode(node->condition.get());
    if (node->thenB) {
        enterScope();
        visitNode(node->thenB.get());
        exitScope();
    }
    if (node->elseB) {
        enterScope();
        visitNode(node->elseB.get());
        exitScope();
    }
}

void SemanticAnalyzer::visitWhile(const WhileNode* node) {
    if (node->condition) visitNode(node->condition.get());
    if (node->body) {
        enterScope();
        visitNode(node->body.get());
        exitScope();
    }
}

void SemanticAnalyzer::visitFor(const ForNode* node) {
    enterScope();
    if (node->init) visitNode(node->init.get());
    if (node->condition) visitNode(node->condition.get());
    if (node->step) visitNode(node->step.get());
    if (node->body) visitNode(node->body.get());
    exitScope();
}

void SemanticAnalyzer::visitSwitch(const SwitchNode* node) {
    if (node->expr) visitNode(node->expr.get());
    for (const auto& c : node->cases) {
        enterScope();
        if (c.first) visitNode(c.first.get());
        if (c.second) visitNode(c.second.get());
        exitScope();
    }
    if (node->defaultCase) {
        enterScope();
        visitNode(node->defaultCase.get());
        exitScope();
    }
}

void SemanticAnalyzer::visitBinOp(const BinOpNode* node) {
    if (node->left) visitNode(node->left.get());
    if (node->right) visitNode(node->right.get());
}

void SemanticAnalyzer::visitArrayDecl(const ArrayDeclNode* node) {
    if (node->sizeNode) visitNode(node->sizeNode.get());
    Symbol sym;
    sym.name = node->name;
    sym.type = "array";
    sym.kind = SymbolKind::Variable;
    sym.declRange = node->range;
    sym.documentation = "array " + node->name;
    sym.fileUri = currentFile;
    currentScope->symbols[node->name] = sym;
    symbolRefs.push_back({node->range, sym});
}

void SemanticAnalyzer::visitUsing(const UsingNode* node) {
    loadModuleSymbols(node->libName, node->range);
    DocumentSymbolInfo docSym;
    docSym.name = "using " + node->libName;
    docSym.kind = "Module";
    docSym.range = node->range;
    docSym.selectionRange = node->range;
    documentSymbols.push_back(docSym);
}

void SemanticAnalyzer::visitInclude(const IncludeNode* node) {
    loadModuleSymbols(node->filename, node->range);
    DocumentSymbolInfo docSym;
    docSym.name = "include " + node->filename;
    docSym.kind = "Module";
    docSym.range = node->range;
    docSym.selectionRange = node->range;
    documentSymbols.push_back(docSym);
}

const Symbol* SemanticAnalyzer::findFunction(const std::string& name) const {
    if (currentScope) {
        Symbol* s = currentScope->find(name);
        if (s && (s->kind == SymbolKind::Function || s->kind == SymbolKind::Builtin)) {
            return s;
        }
    }
    if (rootScope) {
        auto it = rootScope->symbols.find(name);
        if (it != rootScope->symbols.end()) {
            if (it->second.kind == SymbolKind::Function || it->second.kind == SymbolKind::Builtin) {
                return &it->second;
            }
        }
    }
    return nullptr;
}

SignatureHelpResult SemanticAnalyzer::getSignatureHelp(const std::string& code, int line, int col) const {
    // 1. Calculate offset in code string
    int curLine = 1;
    int curCol = 1;
    size_t offset = 0;
    for (size_t i = 0; i < code.size(); i++) {
        if (curLine == line && curCol >= col) {
            offset = i;
            break;
        }
        if (code[i] == '\n') {
            curLine++;
            curCol = 1;
        } else {
            curCol++;
        }
        offset = i + 1;
    }

    // 2. Scan backward to find unclosed '(' and count commas
    int parenDepth = 0;
    int commaCount = 0;
    size_t openParenIdx = std::string::npos;
    bool inString = false;

    for (int i = static_cast<int>(offset) - 1; i >= 0; i--) {
        char c = code[i];
        if (c == '"' && (i == 0 || code[i - 1] != '\\')) {
            inString = !inString;
            continue;
        }
        if (inString) continue;

        if (c == ')') {
            parenDepth++;
        } else if (c == '(') {
            if (parenDepth > 0) {
                parenDepth--;
            } else {
                openParenIdx = i;
                break;
            }
        } else if (c == ',' && parenDepth == 0) {
            commaCount++;
        } else if (c == ';' || c == '{' || c == '}') {
            break;
        }
    }

    if (openParenIdx == std::string::npos) {
        return {};
    }

    // 3. Find identifier right before open parenthesis
    int idx = static_cast<int>(openParenIdx) - 1;
    while (idx >= 0 && (code[idx] == ' ' || code[idx] == '\t' || code[idx] == '\r' || code[idx] == '\n')) {
        idx--;
    }
    int endId = idx + 1;
    while (idx >= 0 && (isalnum(static_cast<unsigned char>(code[idx])) || code[idx] == '_')) {
        idx--;
    }
    int startId = idx + 1;
    if (startId >= endId) {
        return {};
    }

    std::string funcName = code.substr(startId, endId - startId);
    const Symbol* fnSym = findFunction(funcName);
    if (!fnSym) {
        return {};
    }

    SignatureHelpResult result;
    result.found = true;
    result.activeSignature = 0;
    result.activeParameter = commaCount;

    SignatureInfo sig;
    std::ostringstream sigLabel;
    sigLabel << fnSym->name << "(";
    for (size_t p = 0; p < fnSym->params.size(); p++) {
        if (p > 0) sigLabel << ", ";
        std::string pLabel = fnSym->params[p].type + " " + fnSym->params[p].name;
        sigLabel << pLabel;
        ParameterInfo paramInfo;
        paramInfo.label = pLabel;
        paramInfo.documentation = "Параметр `" + fnSym->params[p].name + "` (" + fnSym->params[p].type + ")";
        sig.parameters.push_back(std::move(paramInfo));
    }
    sigLabel << ") -> " << fnSym->returnType;
    sig.label = sigLabel.str();
    sig.documentation = fnSym->documentation;

    result.signatures.push_back(std::move(sig));
    return result;
}

HoverInfo SemanticAnalyzer::getHover(int line, int col) const {
    const SymbolRef* best = nullptr;
    for (const auto& ref : symbolRefs) {
        if (ref.range.contains(line, col)) {
            // Find most specific (innermost) match
            if (!best || (ref.range.end.line - ref.range.start.line < best->range.end.line - best->range.start.line) ||
                (ref.range.end.column - ref.range.start.column < best->range.end.column - best->range.start.column)) {
                best = &ref;
            }
        }
    }

    if (!best) return {};

    HoverInfo info;
    info.found = true;
    info.range = best->range;
    std::ostringstream ss;
    ss << "```foxlang\n";
    if (best->symbol.kind == SymbolKind::Function || best->symbol.kind == SymbolKind::Builtin) {
        ss << "(function) " << best->symbol.name << "(";
        for (size_t i = 0; i < best->symbol.params.size(); i++) {
            if (i > 0) ss << ", ";
            ss << best->symbol.params[i].type << " " << best->symbol.params[i].name;
        }
        ss << ") -> " << best->symbol.returnType;
    } else {
        ss << "(variable) " << best->symbol.type << " " << best->symbol.name;
    }
    ss << "\n```";
    if (!best->symbol.documentation.empty() && best->symbol.documentation != best->symbol.name) {
        ss << "\n\n---\n" << best->symbol.documentation;
    }
    info.markdown = ss.str();
    return info;
}

DefinitionInfo SemanticAnalyzer::getDefinition(int line, int col) const {
    for (const auto& ref : symbolRefs) {
        if (ref.range.contains(line, col)) {
            if (ref.symbol.declRange.start.line > 0) {
                DefinitionInfo info;
                info.found = true;
                info.range = ref.symbol.declRange;
                info.fileUri = ref.symbol.fileUri.empty() ? currentFile : ref.symbol.fileUri;
                return info;
            }
        }
    }
    return {};
}

std::vector<CompletionItem> SemanticAnalyzer::getCompletions(int line, int col) const {
    (void)line;
    (void)col;
    std::vector<CompletionItem> items;
    std::unordered_set<std::string> seen;

    auto add = [&](std::string label, std::string kind, std::string detail, std::string doc) {
        if (seen.insert(label).second) {
            items.push_back({std::move(label), std::move(kind), std::move(detail), std::move(doc)});
        }
    };

    // 1. Language keywords and directives
    struct KeywordDoc {
        const char* kw;
        const char* detail;
        const char* doc;
    };
    static const std::vector<KeywordDoc> kwDocs = {
        {"if", "(keyword) if (cond) { ... }", "Условный оператор ветвления `if / else`.\n\n```foxlang\nif (условие) {\n    // истина\n} else {\n    // иначе\n}\n```"},
        {"else", "(keyword) else", "Ветка `else` для оператора ветвления `if`.\n\n```foxlang\nif (cond) {\n    ...\n} else {\n    ...\n}\n```"},
        {"while", "(keyword) while (cond) { ... }", "Цикл с предусловием `while`.\n\n```foxlang\nwhile (условие) {\n    // тело цикла\n}\n```"},
        {"for", "(keyword) for (init; cond; step) { ... }", "Цикл со счётчиком `for`.\n\n```foxlang\nfor (int i = 0; i < 10; i++) {\n    print(i);\n}\n```"},
        {"switch", "(keyword) switch (val) { case ... }", "Оператор множественного выбора `switch / case / default`.\n\n```foxlang\nswitch (val) {\n    case 1: { ... break; }\n    default: { ... }\n}\n```"},
        {"case", "(keyword) case value:", "Ветка выбора `case` внутри оператора `switch`."},
        {"default", "(keyword) default:", "Ветка по умолчанию `default` внутри `switch`."},
        {"break", "(keyword) break;", "Прерывание выполнения текущего цикла или оператора `switch`."},
        {"continue", "(keyword) continue;", "Переход к следующей итерации цикла."},
        {"return", "(keyword) return [value];", "Возврат значения из функции или выход из `void` функции."},
        {"using", "(keyword) using <module>;", "Директива подключения стандартной библиотеки FoxLang.\n\n```foxlang\nusing server;\nusing http;\nusing env;\nusing log;\nusing json;\nusing string;\n```"},
        {"include", "(keyword) include(\"path.fox\");", "Директива подключения пользовательского файла с кодом.\n\n```foxlang\ninclude(\"helper.fox\");\n```"},
        {"global", "(keyword) global type name = val;", "Объявление глобальной переменной в FoxLang.\n\n```foxlang\nglobal string token = secret(\"API_KEY\");\n```"},
        {"int", "(type) int", "32-битное целое число со знаком."},
        {"float", "(type) float", "Дробное число с плавающей точкой."},
        {"string", "(type) string", "Текстовая строка с поддержкой UTF-8 и Unicode эмодзи."},
        {"bool", "(type) bool", "Логический тип данных: `true` или `false`."},
        {"void", "(type) void", "Тип отсутствия возвращаемого значения функции."},
        {"true", "(keyword) true", "Логическая истина."},
        {"false", "(keyword) false", "Логическая ложь."},
        {"array", "(keyword) array <name> <size>;", "Объявление массива фиксированного размера: `array имя размер;`."}
    };
    for (const auto& kd : kwDocs) {
        add(kd.kw, "Keyword", kd.detail, kd.doc);
    }

    // 2. Standard library modules
    struct ModuleDoc {
        const char* name;
        const char* doc;
    };
    static const std::vector<ModuleDoc> stdModules = {
        {"server", "Модуль HTTP/webhook сервера для Linux и Windows (`listen`, `get`, `post`, `body`, `method`, `path`, `respond`, `respond_status`)."},
        {"http", "Модуль исходящих HTTP-клиентских запросов (`http_fetch`, `http_post_json`, `http_post_as`, `http_put_json`, `http_remove`)."},
        {"env", "Модуль переменных окружения и секретов (`env`, `secret`, `env_default`). Автоматически читает `.env` файл."},
        {"log", "Модуль уровневого логирования (`debug`, `info`, `warn`, `error`)."},
        {"json", "Модуль работы с JSON (`json_path`, `json_safe`, поддержка UTF-16 surrogate pairs и emoji)."},
        {"string", "Модуль строковых операций (`contains`, `replace`, `to_int`, `strtoint`)."},
        {"math", "Модуль математики (`clamp01`, `min_int`, `max_int`)."},
        {"net", "Модуль низкоуровневых сокетов и DNS (`connect_tcp`, `send_tcp`, `recv_tcp`, `close_tcp`, `resolve_host`)."},
        {"terminal", "Модуль TUI и ANSI-графики (`clear`, `home`, `write`, `goto_xy`, `color`, `reset_color`)."},
        {"time", "Модуль времени и задержки (`sleep_ms`, `unix_time_ms`)."}
    };
    for (const auto& md : stdModules) {
        add(md.name, "Module", std::string("(module) using ") + md.name + ";", md.doc);
    }

    // 3. All visible symbols from root scope and symbol refs
    std::vector<Symbol> allSymbols;
    rootScope->getAllSymbols(allSymbols);
    for (const auto& ref : symbolRefs) {
        allSymbols.push_back(ref.symbol);
    }

    for (const auto& sym : allSymbols) {
        std::string kindStr = (sym.kind == SymbolKind::Function || sym.kind == SymbolKind::Builtin) ? "Function" : "Variable";
        std::string detail;
        if (sym.kind == SymbolKind::Function || sym.kind == SymbolKind::Builtin) {
            std::ostringstream ss;
            ss << "(function) " << sym.name << "(";
            for (size_t i = 0; i < sym.params.size(); i++) {
                if (i > 0) ss << ", ";
                ss << sym.params[i].type << " " << sym.params[i].name;
            }
            ss << ") -> " << sym.returnType;
            detail = ss.str();
        } else {
            detail = "(variable) " + sym.type + " " + sym.name;
        }
        add(sym.name, kindStr, detail, sym.documentation);
    }

    return items;
}

std::vector<DocumentSymbolInfo> SemanticAnalyzer::getDocumentSymbols() const {
    return documentSymbols;
}

} // namespace foxlang
