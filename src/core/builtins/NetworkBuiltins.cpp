// HTTP client, TCP/DNS and HTTP server builtins.
#include "Builtin.h"
#include "foxlang/Platform.h"
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
         "Регистрирует функцию-обработчик для метода и точного пути (без query-строки)."},
        [](Call& c) {
            std::string method = upper(c.text(0));
            if (!isMethod(method)) throw std::runtime_error("HTTP Server Error: invalid method '" + c.text(0) + "'");
            if (c.text(1).empty() || c.text(1)[0] != '/')
                throw std::runtime_error("HTTP Server Error: route path must start with '/': '" + c.text(1) + "'");
            platform::serverState(c.ctx).routes[method + " " + c.text(1)] = c.text(2);
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
            long long status = c.integer(0);
            if (status < 100 || status > 599) throw std::runtime_error("HTTP Server Error: status must be 100..599");
            std::string type = c.has(2) ? c.text(2) : "application/json; charset=utf-8";
            if (type.find_first_of("\r\n") != std::string::npos || type.find('\0') != std::string::npos)
                throw std::runtime_error("HTTP Server Error: invalid Content-Type");
            auto& state = platform::serverState(c.ctx);
            state.status = static_cast<int>(status);
            state.response = c.text(1);
            state.contentType = type;
            return nothing();
        });
    add({"request_method", "string", {}, 0, false, "server", "Метод текущего запроса: `GET`, `POST` и т. д."},
        [](Call& c) { return text(platform::serverState(c.ctx).request.method); });
    add({"request_path", "string", {}, 0, false, "server", "Путь текущего запроса без query-строки."},
        [](Call& c) { return text(platform::serverState(c.ctx).request.path); });
    add({"request_query", "string", {}, 0, false, "server", "Query-строка текущего запроса без `?` (например, `page=2&q=fox`)."},
        [](Call& c) { return text(platform::serverState(c.ctx).request.query); });
    add({"request_body", "string", {}, 0, false, "server", "Тело текущего запроса."},
        [](Call& c) { return text(platform::serverState(c.ctx).request.body); });
    add({"request_header", "string", {{"string", "name"}}, 1, false, "server",
         "Значение заголовка текущего запроса без учёта регистра имени; пустая строка, если его нет."},
        [](Call& c) {
            std::string name = c.text(0);
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            const auto& headers = platform::serverState(c.ctx).request.headers;
            auto found = headers.find(name);
            return text(found == headers.end() ? "" : found->second);
        });
}

} // namespace foxlang::runtime
