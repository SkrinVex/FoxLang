#include "LspServer.h"
#include "foxlang/FoxLang.h"
#include <iostream>

namespace foxlang {
namespace lsp {

LspServer::LspServer(Transport& t) : transport(t) {}

int LspServer::run(std::istream& in, std::ostream& out) {
    std::string message;
    while (!exitRequested && transport.readMessage(in, message)) {
        processMessage(message, out);
    }
    return 0;
}

void LspServer::processMessage(const std::string& rawMessage, std::ostream& out) {
    try {
        JsonValue msg = JsonValue::parse(rawMessage);
        if (!msg.isObject()) return;

        if (msg.has("id")) {
            handleRequest(msg, out);
        } else if (msg.has("method")) {
            handleNotification(msg, out);
        }
    } catch (const std::exception& e) {
        Transport::log(std::string("Exception processing message: ") + e.what());
    }
}

void LspServer::handleRequest(const JsonValue& msg, std::ostream& out) {
    std::string method = msg.get("method").asString();
    JsonValue id = msg.get("id");

    if (method == "initialize") {
        std::map<std::string, JsonValue> capabilities;
        capabilities["textDocumentSync"] = 1; // Full document sync

        std::map<std::string, JsonValue> compProvider;
        compProvider["resolveProvider"] = false;
        std::vector<JsonValue> triggers;
        triggers.push_back(JsonValue("."));
        triggers.push_back(JsonValue(" "));
        compProvider["triggerCharacters"] = JsonValue(triggers);
        capabilities["completionProvider"] = JsonValue(compProvider);

        capabilities["hoverProvider"] = true;
        capabilities["definitionProvider"] = true;
        capabilities["documentSymbolProvider"] = true;

        std::map<std::string, JsonValue> serverInfo;
        serverInfo["name"] = "foxlang-lsp";
        serverInfo["version"] = foxlang::Interpreter::getVersion();

        std::map<std::string, JsonValue> res;
        res["capabilities"] = JsonValue(capabilities);
        res["serverInfo"] = JsonValue(serverInfo);

        sendResponse(id, JsonValue(res), out);
        isInitialized = true;
        return;
    }

    if (method == "shutdown") {
        isShutdown = true;
        sendResponse(id, JsonValue(), out);
        return;
    }

    if (method == "textDocument/completion") {
        const auto& params = msg.get("params");
        std::string uri = params.get("textDocument").get("uri").asString();
        LspPosition pos = LspPosition::fromJson(params.get("position"));
        const auto* doc = docManager.getDocument(uri);
        std::vector<JsonValue> items;
        if (doc && doc->analyzer) {
            int line = utf::lspLineToLine(pos.line);
            int col = utf::lspCharacterToColumn(pos.character);
            auto compItems = doc->analyzer->getCompletions(line, col);
            for (const auto& ci : compItems) {
                int kindVal = 1;
                if (ci.kind == "Keyword") kindVal = 14;
                else if (ci.kind == "Function") kindVal = 3;
                else if (ci.kind == "Variable") kindVal = 6;
                else if (ci.kind == "Type") kindVal = 7;
                LspCompletionItem lci;
                lci.label = ci.label;
                lci.kind = kindVal;
                lci.detail = ci.detail;
                lci.documentation = ci.documentation;
                items.push_back(lci.toJson());
            }
        }
        sendResponse(id, JsonValue(items), out);
        return;
    }

    if (method == "textDocument/hover") {
        const auto& params = msg.get("params");
        std::string uri = params.get("textDocument").get("uri").asString();
        LspPosition pos = LspPosition::fromJson(params.get("position"));
        const auto* doc = docManager.getDocument(uri);
        if (doc && doc->analyzer) {
            int line = utf::lspLineToLine(pos.line);
            int col = utf::lspCharacterToColumn(pos.character);
            auto hover = doc->analyzer->getHover(line, col);
            if (hover.found) {
                std::map<std::string, JsonValue> res;
                std::map<std::string, JsonValue> contents;
                contents["kind"] = "markdown";
                contents["value"] = hover.markdown;
                res["contents"] = JsonValue(contents);
                res["range"] = LspRange::fromSourceRange(hover.range).toJson();
                sendResponse(id, JsonValue(res), out);
                return;
            }
        }
        sendResponse(id, JsonValue(), out);
        return;
    }

    if (method == "textDocument/definition") {
        const auto& params = msg.get("params");
        std::string uri = params.get("textDocument").get("uri").asString();
        LspPosition pos = LspPosition::fromJson(params.get("position"));
        const auto* doc = docManager.getDocument(uri);
        if (doc && doc->analyzer) {
            int line = utf::lspLineToLine(pos.line);
            int col = utf::lspCharacterToColumn(pos.character);
            auto def = doc->analyzer->getDefinition(line, col);
            if (def.found) {
                std::string targetUri = def.fileUri;
                if (targetUri.compare(0, 7, "file://") != 0 && !targetUri.empty()) {
                    targetUri = "file://" + targetUri;
                }
                if (targetUri.empty()) targetUri = uri;
                LspLocation loc;
                loc.uri = targetUri;
                loc.range = LspRange::fromSourceRange(def.range);
                sendResponse(id, loc.toJson(), out);
                return;
            }
        }
        sendResponse(id, JsonValue(), out);
        return;
    }

    if (method == "textDocument/documentSymbol") {
        const auto& params = msg.get("params");
        std::string uri = params.get("textDocument").get("uri").asString();
        const auto* doc = docManager.getDocument(uri);
        std::vector<JsonValue> syms;
        if (doc && doc->analyzer) {
            auto docSyms = doc->analyzer->getDocumentSymbols();
            for (const auto& ds : docSyms) {
                int kindVal = 13; // Variable
                if (ds.kind == "Function") kindVal = 12;
                else if (ds.kind == "Module") kindVal = 2;
                LspDocumentSymbol lds;
                lds.name = ds.name;
                lds.kind = kindVal;
                lds.range = LspRange::fromSourceRange(ds.range);
                lds.selectionRange = LspRange::fromSourceRange(ds.selectionRange);
                syms.push_back(lds.toJson());
            }
        }
        sendResponse(id, JsonValue(syms), out);
        return;
    }

    // Default unknown method response
    sendError(id, -32601, "Method not found: " + method, out);
}

void LspServer::handleNotification(const JsonValue& msg, std::ostream& out) {
    std::string method = msg.get("method").asString();

    if (method == "initialized") {
        // Notification from client after initialize response
        return;
    }

    if (method == "exit") {
        exitRequested = true;
        return;
    }

    if (method == "textDocument/didOpen") {
        const auto& td = msg.get("params").get("textDocument");
        std::string uri = td.get("uri").asString();
        std::string text = td.get("text").asString();
        int version = td.get("version").asInt(1);
        docManager.openDocument(uri, text, version);
        publishDiagnostics(uri, out);
        return;
    }

    if (method == "textDocument/didChange") {
        const auto& params = msg.get("params");
        const auto& td = params.get("textDocument");
        std::string uri = td.get("uri").asString();
        int version = td.get("version").asInt(1);
        const auto& changes = params.get("contentChanges");
        if (changes.isArray() && !changes.arrayValue.empty()) {
            std::string text = changes[0].get("text").asString();
            docManager.updateDocument(uri, text, version);
            publishDiagnostics(uri, out);
        }
        return;
    }

    if (method == "textDocument/didClose") {
        std::string uri = msg.get("params").get("textDocument").get("uri").asString();
        docManager.closeDocument(uri);
        // Clear diagnostics on close
        std::map<std::string, JsonValue> params;
        params["uri"] = uri;
        params["diagnostics"] = JsonValue(std::vector<JsonValue>{});
        sendNotification("textDocument/publishDiagnostics", JsonValue(params), out);
        return;
    }
}

void LspServer::sendResponse(const JsonValue& id, const JsonValue& result, std::ostream& out) {
    std::map<std::string, JsonValue> res;
    res["jsonrpc"] = "2.0";
    res["id"] = id;
    res["result"] = result;
    transport.sendMessage(out, JsonValue(res).serialize());
}

void LspServer::sendError(const JsonValue& id, int code, const std::string& message, std::ostream& out) {
    std::map<std::string, JsonValue> errObj;
    errObj["code"] = code;
    errObj["message"] = message;

    std::map<std::string, JsonValue> res;
    res["jsonrpc"] = "2.0";
    res["id"] = id;
    res["error"] = JsonValue(errObj);
    transport.sendMessage(out, JsonValue(res).serialize());
}

void LspServer::sendNotification(const std::string& method, const JsonValue& params, std::ostream& out) {
    std::map<std::string, JsonValue> notif;
    notif["jsonrpc"] = "2.0";
    notif["method"] = method;
    notif["params"] = params;
    transport.sendMessage(out, JsonValue(notif).serialize());
}

void LspServer::publishDiagnostics(const std::string& uri, std::ostream& out) {
    auto diags = docManager.getDiagnostics(uri);
    std::vector<JsonValue> diagJson;
    for (const auto& d : diags) {
        diagJson.push_back(d.toJson());
    }
    std::map<std::string, JsonValue> params;
    params["uri"] = uri;
    params["diagnostics"] = JsonValue(diagJson);
    sendNotification("textDocument/publishDiagnostics", JsonValue(params), out);
}

} // namespace lsp
} // namespace foxlang
