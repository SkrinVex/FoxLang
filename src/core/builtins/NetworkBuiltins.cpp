// HTTP client, TCP/DNS and HTTP server builtins.
#include "Builtin.h"
#include "../HttpServer.h"
#include "foxlang/Platform.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <stdexcept>

namespace foxlang::runtime {
namespace {

int& lastStatus() {
    static thread_local int status = 0;
    return status;
}

std::string upper(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return text;
}

bool isMethod(const std::string& method) {
    return !method.empty() && method.size() <= 16 &&
           std::all_of(method.begin(), method.end(), [](unsigned char ch) { return ch >= 'A' && ch <= 'Z'; });
}

int status(Call& c, size_t index) {
    long long value = c.integer(index);
    if (value < 100 || value > 599) throw std::runtime_error("HTTP Server Error: status must be 100..599");
    return static_cast<int>(value);
}

const std::string& headerValue(Call& c, size_t index) {
    if (!http::validHeaderText(c.text(index)))
        throw std::runtime_error("HTTP Server Error: " + c.what(index) + " must not contain line breaks");
    return c.text(index);
}

void listen(Call& c, const std::string& certificate, const std::string& key) {
    auto port = static_cast<int>(c.integer(0));
    platform::runHttpServer(port, *c.ctx.getRoot(), nullptr, certificate, key);
}

} // namespace

void addNetworkBuiltins(std::vector<Builtin>& out) {
    auto add = [&](BuiltinSpec spec, Handler handler) { out.push_back({std::move(spec), handler}); };

    add({"http_request", "string",
         {{"string", "method"}, {"string", "url"}, {"string", "body"}, {"string", "content_type"}}, 2, false, "http",
         "Исходящий HTTP(S)-запрос любым методом. Возвращает тело ответа при любом коде статуса; "
         "код доступен через `http_status()`. При сетевой ошибке или ошибке TLS — пустая строка и статус `0`, "
         "причина пишется в stderr. `content_type` по умолчанию `application/json`."},
        [](Call& c) {
            std::string method = upper(c.text(0));
            std::string body = c.has(2) ? c.text(2) : "";
            std::string type = c.has(3) ? c.text(3) : "application/json";
            try {
                auto response = platform::httpRequest(method, c.text(1), body, type);
                lastStatus() = response.status;
                return text(std::move(response.body));
            } catch (const std::exception& error) {
                lastStatus() = 0;
                std::cerr << "[HTTP ERROR] " << error.what() << std::endl;
                return text("");
            }
        });
    add({"http_status", "int", {}, 0, false, "http",
         "Код статуса последнего `http_request` (например, `200` или `404`); `0`, если ответа не было."},
        [](Call&) { return integer(lastStatus()); });

    add({"tcp_connect", "int", {{"string", "host"}, {"int", "port"}}, 2, false, "net",
         "Открывает TCP-соединение. Возвращает дескриптор или `-1` при ошибке."},
        [](Call& c) {
            long long port = c.integer(1);
            if (port < 1 || port > 65535) return integer(-1);
            return integer(platform::tcpConnect(c.text(0), static_cast<int>(port)));
        });
    add({"tcp_send", "int", {{"int", "socket"}, {"string", "data"}}, 2, false, "net",
         "Отправляет данные в сокет. Возвращает число байт или `-1` при ошибке."},
        [](Call& c) { return integer(platform::tcpSend(static_cast<int>(c.integer(0)), c.text(1))); });
    add({"tcp_recv", "string", {{"int", "socket"}, {"int", "max_bytes"}}, 2, false, "net",
         "Читает до `max_bytes` байт. Пустая строка — соединение закрыто или ошибка чтения."},
        [](Call& c) { return text(platform::tcpRecv(static_cast<int>(c.integer(0)), static_cast<int>(c.integer(1)))); });
    add({"tcp_close", "bool", {{"int", "socket"}}, 1, false, "net", "Закрывает сокет; `true`, если он был открыт."},
        [](Call& c) { return boolean(platform::tcpClose(static_cast<int>(c.integer(0)))); });
    add({"dns_lookup", "string", {{"string", "host"}}, 1, false, "net",
         "IP-адрес хоста через DNS или пустая строка, если имя не найдено."},
        [](Call& c) { return text(platform::dnsLookup(c.text(0))); });

    add({"server_route", "void", {{"string", "method"}, {"string", "path"}, {"string", "handler"}}, 3, false, "server",
         "Регистрирует функцию-обработчик для метода и пути. Сегмент `:имя` совпадает с одной частью пути, "
         "`*имя` в конце — с остатком; значения читает `request_param`. Точные пути проверяются раньше шаблонов."},
        [](Call& c) {
            std::string method = upper(c.text(0));
            if (!isMethod(method)) throw std::runtime_error("HTTP Server Error: invalid method '" + c.text(0) + "'");
            const std::string& pattern = c.text(1);
            if (pattern.empty() || pattern[0] != '/')
                throw std::runtime_error("HTTP Server Error: route path must start with '/': '" + pattern + "'");
            platform::HttpRoute route{method, pattern, {}, c.text(2)};
            std::stringstream parts(pattern);
            for (std::string part; std::getline(parts, part, '/');) {
                if (part.empty()) continue;
                if (!route.segments.empty() && route.segments.back()[0] == '*')
                    throw std::runtime_error("HTTP Server Error: '*' must be the last segment of '" + pattern + "'");
                route.segments.push_back(part);
            }
            auto& routes = platform::serverState(c.ctx).routes;
            // Registering the same method and path again replaces the handler.
            auto same = std::find_if(routes.begin(), routes.end(), [&](const platform::HttpRoute& r) {
                return r.method == route.method && r.segments == route.segments;
            });
            if (same != routes.end()) *same = route;
            else routes.push_back(route);
            return nothing();
        });
    add({"server_static", "void", {{"string", "prefix"}, {"string", "directory"}}, 2, false, "server",
         "Раздаёт файлы каталога по URL с префиксом: `server_static(\"/\", \"public\")` отдаёт `public/style.css` "
         "по `/style.css`, для каталога — `index.html`. Тип содержимого определяется по расширению. "
         "Скрытые файлы (`.env`) и выход за пределы каталога не отдаются. Маршруты проверяются раньше статики."},
        [](Call& c) {
            if (c.text(0).empty() || c.text(0)[0] != '/')
                throw std::runtime_error("HTTP Server Error: static prefix must start with '/': '" + c.text(0) + "'");
            platform::serverState(c.ctx).staticMounts.push_back({c.text(0), c.text(1)});
            return nothing();
        });
    add({"server_not_found", "void", {{"string", "handler"}}, 1, false, "server",
         "Функция, которая отвечает, когда не подошёл ни маршрут, ни файл; статус по умолчанию 404."},
        [](Call& c) {
            platform::serverState(c.ctx).notFoundHandler = c.text(0);
            return nothing();
        });
    add({"server_cors", "void", {{"string", "origin"}}, 1, false, "server",
         "Разрешает запросы из браузера с другого домена: добавляет заголовки CORS ко всем ответам и отвечает на "
         "предварительные запросы `OPTIONS`. `\"*\"` — любой домен."},
        [](Call& c) {
            if (!http::validHeaderText(c.text(0))) throw std::runtime_error("HTTP Server Error: invalid CORS origin");
            platform::serverState(c.ctx).corsOrigin = c.text(0);
            return nothing();
        });
    add({"server_access_log", "void", {{"bool", "enabled"}}, 1, false, "server",
         "Печатать ли в stderr строку о каждом запросе: адрес, метод, путь, статус и время обработки."},
        [](Call& c) {
            platform::serverState(c.ctx).accessLog = c.flag(0);
            return nothing();
        });
    add({"server_max_body", "void", {{"int", "bytes"}}, 1, false, "server",
         "Наибольший размер тела запроса в байтах (по умолчанию 10 МиБ). Больший запрос получает ответ 413."},
        [](Call& c) {
            platform::serverState(c.ctx).maxBody = c.amount(0, size_t{1} << 30);
            return nothing();
        });
    add({"server_listen", "void", {{"int", "port"}}, 1, false, "server",
         "Запускает HTTP-сервер на 0.0.0.0 и обрабатывает запросы, пока не вызван `server_stop()`."},
        [](Call& c) {
            listen(c, "", "");
            return nothing();
        });
    add({"server_listen_tls", "void", {{"int", "port"}, {"string", "certificate"}, {"string", "private_key"}}, 3, false, "server",
         "Запускает HTTPS-сервер (TLS 1.2+). Аргументы — пути к PEM-сертификату с цепочкой и незашифрованному ключу."},
        [](Call& c) {
            if (c.text(1).empty() || c.text(2).empty())
                throw std::runtime_error("HTTPS Server Error: certificate and private key paths are required");
            listen(c, c.text(1), c.text(2));
            return nothing();
        });
    add({"server_stop", "void", {}, 0, false, "server",
         "Останавливает сервер после ответа на текущий запрос."},
        [](Call& c) {
            platform::serverState(c.ctx).stopRequested = true;
            return nothing();
        });
    add({"server_respond", "void", {{"int", "status"}, {"string", "body"}, {"string", "content_type"}}, 2, false, "server",
         "Задаёт ответ на текущий запрос: код статуса, тело и `Content-Type` "
         "(по умолчанию `application/json; charset=utf-8`)."},
        [](Call& c) {
            auto& reply = platform::serverState(c.ctx).reply;
            reply.status = status(c, 0);
            reply.body = c.text(1);
            reply.contentType = c.has(2) ? headerValue(c, 2) : "application/json; charset=utf-8";
            return nothing();
        });
    add({"server_header", "void", {{"string", "name"}, {"string", "value"}}, 2, false, "server",
         "Добавляет заголовок к ответу на текущий запрос, например `Cache-Control`."},
        [](Call& c) {
            if (c.text(0).empty() || c.text(0).find_first_of(": \t") != std::string::npos)
                throw std::runtime_error("HTTP Server Error: invalid header name '" + c.text(0) + "'");
            platform::serverState(c.ctx).reply.headers.push_back({c.text(0), headerValue(c, 1)});
            return nothing();
        });
    add({"server_redirect", "void", {{"string", "url"}, {"int", "status"}}, 1, false, "server",
         "Перенаправляет клиента на другой адрес: статус 302 (или указанный, например 301) и заголовок `Location`."},
        [](Call& c) {
            auto& reply = platform::serverState(c.ctx).reply;
            reply.status = c.has(1) ? status(c, 1) : 302;
            reply.body.clear();
            reply.headers.push_back({"Location", headerValue(c, 0)});
            return nothing();
        });
    add({"server_send_file", "bool", {{"string", "path"}, {"string", "content_type"}}, 1, false, "server",
         "Отвечает содержимым файла; тип определяется по расширению, если не указан. Нет файла — ответ 404 и `false`."},
        [](Call& c) {
            auto& reply = platform::serverState(c.ctx).reply;
            if (http::sendFile(reply, c.text(0), c.has(1) ? headerValue(c, 1) : "")) return boolean(true);
            reply = {404, "application/json; charset=utf-8", "{\"error\":\"Not Found\"}", {}};
            return boolean(false);
        });
    add({"server_download", "bool", {{"string", "path"}, {"string", "filename"}}, 1, false, "server",
         "Отдаёт файл для скачивания (`Content-Disposition: attachment`) под именем `filename` или собственным. "
         "Нет файла — ответ 404 и `false`."},
        [](Call& c) {
            auto& reply = platform::serverState(c.ctx).reply;
            if (!http::sendFile(reply, c.text(0))) {
                reply = {404, "application/json; charset=utf-8", "{\"error\":\"Not Found\"}", {}};
                return boolean(false);
            }
            std::string name = c.has(1) ? c.text(1) : std::filesystem::u8path(c.text(0)).filename().u8string();
            std::string ascii;
            for (unsigned char ch : name) ascii += (ch < 0x80 && ch >= 0x20 && ch != '"' && ch != '\\') ? static_cast<char>(ch) : '_';
            // filename* carries the real UTF-8 name; filename is the fallback for old clients.
            reply.headers.push_back({"Content-Disposition",
                                     "attachment; filename=\"" + ascii + "\"; filename*=UTF-8''" + http::percentEncode(name)});
            return boolean(true);
        });
    add({"server_set_cookie", "void", {{"string", "name"}, {"string", "value"}, {"string", "options"}}, 2, false, "server",
         "Устанавливает cookie в ответе. `options` — атрибуты через точку с запятой: "
         "`\"Path=/; Max-Age=3600; HttpOnly; SameSite=Lax\"`. Значение кодируется автоматически."},
        [](Call& c) {
            const std::string& name = c.text(0);
            if (name.empty() || name.find_first_of("=; \t\r\n,") != std::string::npos)
                throw std::runtime_error("HTTP Server Error: invalid cookie name '" + name + "'");
            std::string cookie = name + "=" + http::percentEncode(c.text(1));
            if (c.has(2) && !c.text(2).empty()) cookie += "; " + headerValue(c, 2);
            platform::serverState(c.ctx).reply.headers.push_back({"Set-Cookie", cookie});
            return nothing();
        });
    add({"request_method", "string", {}, 0, false, "server", "Метод текущего запроса: `GET`, `POST` и т. д."},
        [](Call& c) { return text(platform::serverState(c.ctx).request.method); });
    add({"request_path", "string", {}, 0, false, "server", "Путь текущего запроса без query-строки, декодированный."},
        [](Call& c) { return text(http::percentDecode(platform::serverState(c.ctx).request.path, false)); });
    add({"request_query", "string", {}, 0, false, "server", "Query-строка текущего запроса без `?`, как пришла (например, `page=2&q=fox`)."},
        [](Call& c) { return text(platform::serverState(c.ctx).request.query); });
    add({"request_query_param", "string", {{"string", "name"}}, 1, false, "server",
         "Значение параметра query-строки, декодированное (`?q=%D0%BB%D0%B8%D1%81` даёт `лис`); пустая строка, если его нет."},
        [](Call& c) {
            auto values = http::parseQuery(platform::serverState(c.ctx).request.query);
            auto found = values.find(c.text(0));
            return text(found == values.end() ? "" : found->second);
        });
    add({"request_param", "string", {{"string", "name"}}, 1, false, "server",
         "Значение сегмента пути из шаблона маршрута: для `/users/:id` и запроса `/users/42` `request_param(\"id\")` даёт `42`."},
        [](Call& c) {
            const auto& params = platform::serverState(c.ctx).request.params;
            auto found = params.find(c.text(0));
            return text(found == params.end() ? "" : found->second);
        });
    add({"request_body", "string", {}, 0, false, "server", "Тело текущего запроса как есть."},
        [](Call& c) { return text(platform::serverState(c.ctx).request.body); });
    add({"request_form", "string", {{"string", "name"}}, 1, false, "server",
         "Поле HTML-формы из тела запроса: `application/x-www-form-urlencoded` или `multipart/form-data`."},
        [](Call& c) { return text(http::formValue(platform::serverState(c.ctx).request, c.text(0))); });
    add({"request_header", "string", {{"string", "name"}}, 1, false, "server",
         "Значение заголовка текущего запроса без учёта регистра имени; пустая строка, если его нет."},
        [](Call& c) {
            std::string name = c.text(0);
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            const auto& headers = platform::serverState(c.ctx).request.headers;
            auto found = headers.find(name);
            return text(found == headers.end() ? "" : found->second);
        });
    add({"request_cookie", "string", {{"string", "name"}}, 1, false, "server",
         "Значение cookie, присланного клиентом; пустая строка, если его нет."},
        [](Call& c) {
            const auto& headers = platform::serverState(c.ctx).request.headers;
            auto header = headers.find("cookie");
            if (header == headers.end()) return text("");
            auto cookies = http::parseCookies(header->second);
            auto found = cookies.find(c.text(0));
            return text(found == cookies.end() ? "" : found->second);
        });
    add({"request_ip", "string", {}, 0, false, "server", "IP-адрес клиента текущего запроса."},
        [](Call& c) { return text(platform::serverState(c.ctx).request.clientIp); });
    add({"request_file_name", "string", {{"string", "field"}}, 1, false, "server",
         "Исходное имя файла, загруженного через поле формы `multipart/form-data`; пустая строка, если файла нет."},
        [](Call& c) {
            for (const auto& part : http::parseMultipart(platform::serverState(c.ctx).request))
                if (part.name == c.text(0) && !part.filename.empty()) return text(part.filename);
            return text("");
        });
    add({"request_file_save", "bool", {{"string", "field"}, {"string", "path"}}, 2, false, "server",
         "Сохраняет файл, загруженный через поле формы, по указанному пути. `false`, если файла нет или запись не удалась. "
         "Не используйте присланное имя файла как путь без проверки."},
        [](Call& c) {
            for (const auto& part : http::parseMultipart(platform::serverState(c.ctx).request)) {
                if (part.name != c.text(0) || part.filename.empty()) continue;
                std::ofstream file(std::filesystem::u8path(c.text(1)), std::ios::binary | std::ios::trunc);
                if (!file) return boolean(false);
                file << part.data;
                file.close();
                return boolean(static_cast<bool>(file));
            }
            return boolean(false);
        });
}

} // namespace foxlang::runtime
