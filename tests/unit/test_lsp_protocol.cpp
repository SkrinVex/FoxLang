#include "Json.h"
#include "Transport.h"
#include "Protocol.h"
#include "LspServer.h"
#include <iostream>
#include <sstream>
#include <cassert>

using namespace foxlang::lsp;

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL: " #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return 1; \
        } \
    } while (0)

int main() {
    Transport transport;
    LspServer server(transport);

    std::stringstream clientToServer;
    std::stringstream serverToClient;

    auto sendClientMessage = [&](const std::string& json) {
        transport.sendMessage(clientToServer, json);
    };

    auto readServerMessage = [&]() -> std::string {
        std::string msg;
        if (transport.readMessage(serverToClient, msg)) {
            return msg;
        }
        return "";
    };

    // Helper to feed client messages into server
    auto pump = [&]() {
        std::string raw;
        while (transport.readMessage(clientToServer, raw)) {
            server.processMessage(raw, serverToClient);
        }
        clientToServer.clear();
    };

    // 1. Initialize Request
    {
        sendClientMessage("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"capabilities\":{}}}");
        pump();

        std::string resp = readServerMessage();
        TEST_ASSERT(!resp.empty());
        auto val = JsonValue::parse(resp);
        TEST_ASSERT(val.get("id").asInt() == 1);
        auto caps = val.get("result").get("capabilities");
        TEST_ASSERT(caps.get("hoverProvider").asBool() == true);
        TEST_ASSERT(caps.get("definitionProvider").asBool() == true);
        TEST_ASSERT(caps.get("documentSymbolProvider").asBool() == true);
        TEST_ASSERT(caps.get("completionProvider").isObject());
    }

    // 2. Initialized Notification
    {
        sendClientMessage("{\"jsonrpc\":\"2.0\",\"method\":\"initialized\",\"params\":{}}");
        pump();
        // Initialized notification produces no response
    }

    // 3. textDocument/didOpen (Valid file)
    std::string fileUri = "file:///workspace/main.fox";
    {
        std::string foxCode = "int myScore = 42;\nvoid updateScore() {\n    print(myScore);\n}\n";
        JsonValue params = JsonValue::object();
        JsonValue item = JsonValue::object();
        item.set("uri", JsonValue(fileUri));
        item.set("languageId", JsonValue("fox"));
        item.set("version", JsonValue(1));
        item.set("text", JsonValue(foxCode));
        params.set("textDocument", item);

        JsonValue openMsg = JsonValue::object();
        openMsg.set("jsonrpc", JsonValue("2.0"));
        openMsg.set("method", JsonValue("textDocument/didOpen"));
        openMsg.set("params", params);

        sendClientMessage(openMsg.serialize());
        pump();

        // Server should publish empty diagnostics
        std::string diagMsg = readServerMessage();
        TEST_ASSERT(!diagMsg.empty());
        auto diagVal = JsonValue::parse(diagMsg);
        TEST_ASSERT(diagVal.get("method").asString() == "textDocument/publishDiagnostics");
        auto dList = diagVal.get("params").get("diagnostics").asArray();
        TEST_ASSERT(dList.empty());
    }

    // 4. textDocument/hover on variable
    {
        // Hover at line 0, col 5 (myScore)
        std::string req = "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/hover\",\"params\":{\"textDocument\":{\"uri\":\"" + fileUri + "\"},\"position\":{\"line\":0,\"character\":5}}}";
        sendClientMessage(req);
        pump();

        std::string resp = readServerMessage();
        TEST_ASSERT(!resp.empty());
        auto val = JsonValue::parse(resp);
        TEST_ASSERT(val.get("id").asInt() == 2);
        std::string contents = val.get("result").get("contents").get("value").asString();
        TEST_ASSERT(contents.find("int myScore") != std::string::npos);
    }

    // 5. textDocument/hover on function
    {
        // Hover at line 1, col 6 (updateScore)
        std::string req = "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"textDocument/hover\",\"params\":{\"textDocument\":{\"uri\":\"" + fileUri + "\"},\"position\":{\"line\":1,\"character\":6}}}";
        sendClientMessage(req);
        pump();

        std::string resp = readServerMessage();
        TEST_ASSERT(!resp.empty());
        auto val = JsonValue::parse(resp);
        TEST_ASSERT(val.get("id").asInt() == 3);
        std::string contents = val.get("result").get("contents").get("value").asString();
        TEST_ASSERT(contents.find("void updateScore()") != std::string::npos);
    }

    // 6. textDocument/completion
    {
        std::string req = "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"textDocument/completion\",\"params\":{\"textDocument\":{\"uri\":\"" + fileUri + "\"},\"position\":{\"line\":0,\"character\":0}}}";
        sendClientMessage(req);
        pump();

        std::string resp = readServerMessage();
        TEST_ASSERT(!resp.empty());
        auto val = JsonValue::parse(resp);
        TEST_ASSERT(val.get("id").asInt() == 4);
        auto items = val.get("result").asArray();
        TEST_ASSERT(!items.empty());

        bool foundScore = false;
        bool foundWhile = false;
        bool foundPrint = false;
        for (const auto& item : items) {
            std::string label = item.get("label").asString();
            if (label == "myScore") foundScore = true;
            if (label == "while") foundWhile = true;
            if (label == "print") foundPrint = true;
        }
        TEST_ASSERT(foundScore);
        TEST_ASSERT(foundWhile);
        TEST_ASSERT(foundPrint);
    }

    // 7. textDocument/definition
    {
        // Go to definition of myScore from line 2, character 11
        std::string req = "{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"textDocument/definition\",\"params\":{\"textDocument\":{\"uri\":\"" + fileUri + "\"},\"position\":{\"line\":2,\"character\":11}}}";
        sendClientMessage(req);
        pump();

        std::string resp = readServerMessage();
        TEST_ASSERT(!resp.empty());
        auto val = JsonValue::parse(resp);
        TEST_ASSERT(val.get("id").asInt() == 5);
        auto targetRange = val.get("result").get("range");
        // Target is line 0 (0-based)
        TEST_ASSERT(targetRange.get("start").get("line").asInt() == 0);
    }

    // 8. textDocument/documentSymbol
    {
        std::string req = "{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"textDocument/documentSymbol\",\"params\":{\"textDocument\":{\"uri\":\"" + fileUri + "\"}}}";
        sendClientMessage(req);
        pump();

        std::string resp = readServerMessage();
        TEST_ASSERT(!resp.empty());
        auto val = JsonValue::parse(resp);
        TEST_ASSERT(val.get("id").asInt() == 6);
        auto symbols = val.get("result").asArray();
        TEST_ASSERT(!symbols.empty());

        bool hasScoreSym = false;
        bool hasUpdateSym = false;
        for (const auto& sym : symbols) {
            std::string name = sym.get("name").asString();
            if (name == "myScore") hasScoreSym = true;
            if (name == "updateScore") hasUpdateSym = true;
        }
        TEST_ASSERT(hasScoreSym);
        TEST_ASSERT(hasUpdateSym);
    }

    // 9. textDocument/didChange (introducing syntax / semantic error)
    {
        std::string badCode = "int errVar = ;\nvoid broken() { unknownFunction(); }\n";
        std::string req = "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\",\"params\":{\"textDocument\":{\"uri\":\"" + fileUri + "\",\"version\":2},\"contentChanges\":[{\"text\":\"" + "int errVar = ;\\nvoid broken() { unknownFunction(); }\\n" + "\"}]}}";
        sendClientMessage(req);
        pump();

        std::string diagMsg = readServerMessage();
        TEST_ASSERT(!diagMsg.empty());
        auto diagVal = JsonValue::parse(diagMsg);
        TEST_ASSERT(diagVal.get("method").asString() == "textDocument/publishDiagnostics");
        auto dList = diagVal.get("params").get("diagnostics").asArray();
        TEST_ASSERT(!dList.empty());
    }

    // 10. textDocument/didChange (fixing error)
    {
        std::string req = "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\",\"params\":{\"textDocument\":{\"uri\":\"" + fileUri + "\",\"version\":3},\"contentChanges\":[{\"text\":\"int fixed = 1;\\n\"}]}}";
        sendClientMessage(req);
        pump();

        std::string diagMsg = readServerMessage();
        TEST_ASSERT(!diagMsg.empty());
        auto diagVal = JsonValue::parse(diagMsg);
        TEST_ASSERT(diagVal.get("method").asString() == "textDocument/publishDiagnostics");
        auto dList = diagVal.get("params").get("diagnostics").asArray();
        TEST_ASSERT(dList.empty());
    }

    // 10b. textDocument/signatureHelp
    {
        std::string req = "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\",\"params\":{\"textDocument\":{\"uri\":\"" + fileUri + "\",\"version\":4},\"contentChanges\":[{\"text\":\"using env;\\nvoid test() {\\n    string p = env_default(\\\"PORT\\\", \\\"8080\\\");\\n}\\n\"}]}}";
        sendClientMessage(req);
        pump();
        readServerMessage(); // diagnostics

        std::string sigReq = "{\"jsonrpc\":\"2.0\",\"id\":88,\"method\":\"textDocument/signatureHelp\",\"params\":{\"textDocument\":{\"uri\":\"" + fileUri + "\"},\"position\":{\"line\":2,\"character\":35}}}";
        sendClientMessage(sigReq);
        pump();

        std::string sigResp = readServerMessage();
        TEST_ASSERT(!sigResp.empty());
        auto sigVal = JsonValue::parse(sigResp);
        TEST_ASSERT(sigVal.get("id").asInt() == 88);
        auto sigResult = sigVal.get("result");
        TEST_ASSERT(sigResult.isObject());
        auto sigs = sigResult.get("signatures").asArray();
        TEST_ASSERT(!sigs.empty());
        TEST_ASSERT(sigs[0].get("label").asString().find("env_default") != std::string::npos);
        TEST_ASSERT(sigResult.get("activeParameter").asInt() == 1);
    }

    // 11. Shutdown Request
    {
        sendClientMessage("{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"shutdown\"}");
        pump();

        std::string resp = readServerMessage();
        TEST_ASSERT(!resp.empty());
        auto val = JsonValue::parse(resp);
        TEST_ASSERT(val.get("id").asInt() == 7);
        TEST_ASSERT(val.get("result").isNull());
    }

    // 12. Exit Notification
    {
        TEST_ASSERT(!server.shouldExit());
        sendClientMessage("{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}");
        pump();
        TEST_ASSERT(server.shouldExit());
    }

    std::cout << "LSP_PROTOCOL_TEST_OK" << std::endl;
    return 0;
}
