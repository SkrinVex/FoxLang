#include "DebugAdapter.h"
#include "Channel.h"
#include "Json.h"
#include "foxlang/FoxLang.h"
#include "foxlang/Project.h"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <thread>
#include <unordered_map>

namespace foxlang::debug {
namespace {

using lsp::JsonValue;
using Fields = std::map<std::string, JsonValue>;

const char* const programFrameName = "программа";

std::string quoted(const std::string& text, size_t limit) {
    std::string out = "\"";
    size_t shown = 0;
    for (char c : text) {
        if (shown++ >= limit) {
            out += "…";
            break;
        }
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out + "\"";
}

std::string trimmed(const std::string& text) {
    size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return "";
    return text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);
}

// Paths from the editor and identities from the interpreter name a file the same way.
std::string fileKey(const std::string& path) {
    std::string key = canonicalPath(path);
#ifdef _WIN32
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
    return key;
}

// Lines where the interpreter reports a statement: breakpoints elsewhere never hit.
void statementLines(const Node* node, std::set<int>& lines) {
    if (!node) return;
    if (auto* block = dynamic_cast<const BlockNode*>(node)) {
        for (const auto& stmt : block->stmts) {
            if (!stmt) continue;
            if (!dynamic_cast<const FuncDefNode*>(stmt.get()) && stmt->range.start.line > 0) lines.insert(stmt->range.start.line);
            statementLines(stmt.get(), lines);
        }
    } else if (auto* function = dynamic_cast<const FuncDefNode*>(node)) {
        statementLines(function->body.get(), lines);
    } else if (auto* branch = dynamic_cast<const IfNode*>(node)) {
        statementLines(branch->thenB.get(), lines);
        statementLines(branch->elseB.get(), lines);
    } else if (auto* loop = dynamic_cast<const WhileNode*>(node)) {
        statementLines(loop->body.get(), lines);
    } else if (auto* counted = dynamic_cast<const ForNode*>(node)) {
        statementLines(counted->body.get(), lines);
    } else if (auto* choice = dynamic_cast<const SwitchNode*>(node)) {
        for (const auto& item : choice->cases) statementLines(item.second.get(), lines);
        statementLines(choice->defaultCase.get(), lines);
    }
}

struct Breakpoint {
    int id = 0;
    int line = 0;
    std::string condition, hitCondition, logMessage;
    int hits = 0;
};

class Session final : public DebugHook {
public:
    explicit Session(bool standardStreams) : standardStreams_(standardStreams) {}

    void attach(std::unique_ptr<Channel> channel) { channel_ = std::move(channel); }

    void sendOutput(const std::string& text, const std::string& category) {
        sendEvent("output", Fields{{"category", category}, {"output", text}});
    }

    int serve();

    // DebugHook, called on the interpreter thread.
    void statement(const std::string* file, int line) override;
    void enterScope(Context& scope) override;
    void leaveScope() override;
    void enterFunction(const std::string& name, const std::string* file, int line, Context& scope) override;
    void leaveFunction() override;
    void error(const std::string& message, bool caught) override;

private:
    struct Frame {
        std::string name;
        const std::string* file = nullptr;
        int line = 0;
        Context* scope = nullptr;
    };
    struct Reference {
        enum class Kind { Locals, Globals, Container } kind;
        size_t frame = 0;
        // Keeps the array, map or struct alive while the editor looks at it.
        std::shared_ptr<Object> container;
    };
    enum class Step { None, In, Over, Out };

    bool standardStreams_;
    std::unique_ptr<Channel> channel_;
    std::atomic<int> sequence_{1};

    std::mutex mutex_;
    std::condition_variable wake_;
    bool launched_ = false, configured_ = false;
    JsonValue launch_;
    std::map<std::string, std::vector<Breakpoint>> breakpoints_;
    std::atomic<bool> anyBreakpoints_{false};
    std::atomic<bool> breakOnUncaught_{true};
    std::atomic<bool> breakOnCaught_{false};
    int nextBreakpointId_ = 1;
    std::map<int, std::string> sourceReferences_;
    bool paused_ = false, resume_ = false;
    std::deque<std::function<void()>> tasks_;
    std::atomic<bool> pauseRequested_{false};

    // Interpreter-thread state; the editor's thread reads it only through tasks.
    std::unique_ptr<Interpreter> interpreter_;
    std::thread::id interpreterThread_;
    std::vector<Frame> frames_;
    // Where each open block's frame was before the block began.
    std::vector<Frame> outerBlocks_;
    std::unordered_map<const std::string*, std::string> fileKeys_;
    Step step_ = Step::None;
    size_t stepDepth_ = 0;
    bool entry_ = false;
    bool evaluating_ = false;
    bool errorReported_ = false;
    std::string exceptionText_;
    std::vector<Reference> references_;

    // Protocol
    void readRequests();
    void handle(const JsonValue& request);
    void respond(const JsonValue& request, JsonValue body = JsonValue(), const std::string& failure = "");
    void sendEvent(const std::string& name, JsonValue body = JsonValue());
    [[noreturn]] void finish(int code);
    void setBreakpoints(const JsonValue& request);
    void resume(const JsonValue& request, Step step);
    void inspect(const JsonValue& request, const std::function<JsonValue()>& work);

    // Interpreter thread
    bool onInterpreter() const { return std::this_thread::get_id() == interpreterThread_; }
    void stop(const std::string& reason, const std::string& text, const std::vector<int>& hitIds);
    bool breakpointAt(const std::string* file, int line, std::vector<int>& hitIds);
    bool accepts(Breakpoint& breakpoint);
    Value evaluate(const std::string& text, Context& scope, bool statements);
    std::string interpolate(const std::string& message, Context& scope);
    Context& root() { return interpreter_->getContext(); }
    std::string preview(const Value& value, int depth);
    int referenceFor(Reference reference);
    JsonValue variable(const std::string& name, const Value& value);
    JsonValue source(const std::string* file);
    JsonValue stackTrace(const JsonValue& arguments);
    JsonValue scopes(const JsonValue& arguments);
    JsonValue variables(const JsonValue& arguments);
    JsonValue setVariable(const JsonValue& arguments);
    JsonValue evaluateRequest(const JsonValue& arguments);
};

// ---------------------------------------------------------------- protocol

void Session::respond(const JsonValue& request, JsonValue body, const std::string& failure) {
    JsonValue message = JsonValue::object();
    message["seq"] = JsonValue(sequence_++);
    message["type"] = "response";
    message["request_seq"] = JsonValue(request["seq"].asInt());
    message["command"] = request["command"].asString();
    message["success"] = failure.empty();
    if (!failure.empty()) {
        message["message"] = failure;
        body = Fields{{"error", Fields{{"id", 1}, {"format", failure}, {"showUser", false}}}};
    }
    if (!body.isNull()) message["body"] = std::move(body);
    channel_->write(message.serialize());
}

void Session::sendEvent(const std::string& name, JsonValue body) {
    JsonValue message = JsonValue::object();
    message["seq"] = JsonValue(sequence_++);
    message["type"] = "event";
    message["event"] = name;
    if (!body.isNull()) message["body"] = std::move(body);
    channel_->write(message.serialize());
}

void Session::finish(int code) {
    std::cout.flush();
    std::fflush(nullptr);
    std::_Exit(code);
}

void Session::readRequests() {
    std::string text;
    while (channel_->read(text)) {
        JsonValue message;
        try {
            message = JsonValue::parse(text);
        } catch (const std::exception&) {
            continue;
        }
        if (message["type"].asString() == "request") handle(message);
    }
    // The editor is gone: nobody is left to show the program's state.
    finish(0);
}

void Session::handle(const JsonValue& request) {
    const std::string command = request["command"].asString();
    const JsonValue& arguments = request["arguments"];
    if (command == "initialize") {
        JsonValue filters = std::vector<JsonValue>{
            Fields{{"filter", "uncaught"}, {"label", "Необработанные ошибки"}, {"default", true},
                   {"description", "Остановиться на операторе с ошибкой, которую не перехватит try, пока программа ещё цела"}},
            Fields{{"filter", "caught"}, {"label", "Перехваченные ошибки"}, {"default", false},
                   {"description", "Остановиться и на ошибке внутри try, до того как её обработает catch"}}};
        respond(request, Fields{
            {"supportsConfigurationDoneRequest", true},
            {"supportsConditionalBreakpoints", true},
            {"supportsHitConditionalBreakpoints", true},
            {"supportsLogPoints", true},
            {"supportsEvaluateForHovers", true},
            {"supportsSetVariable", true},
            {"supportsExceptionInfoRequest", true},
            {"supportsTerminateRequest", true},
            {"supportsDelayedStackTraceLoading", true},
            {"exceptionBreakpointFilters", filters}});
        sendEvent("initialized");
    } else if (command == "launch") {
        if (arguments["program"].asString().empty()) {
            respond(request, JsonValue(), "Не указана программа (program) для запуска");
            return;
        }
        respond(request);
        std::lock_guard<std::mutex> lock(mutex_);
        launch_ = arguments;
        launched_ = true;
        wake_.notify_all();
    } else if (command == "attach") {
        respond(request, JsonValue(), "FoxLang запускает программу сам: используйте request \"launch\"");
    } else if (command == "setBreakpoints") {
        setBreakpoints(request);
    } else if (command == "setExceptionBreakpoints") {
        bool uncaught = false, caught = false;
        for (const auto& filter : arguments["filters"].asArray()) {
            uncaught = uncaught || filter.asString() == "uncaught";
            caught = caught || filter.asString() == "caught";
        }
        breakOnUncaught_ = uncaught;
        breakOnCaught_ = caught;
        respond(request);
    } else if (command == "setFunctionBreakpoints") {
        respond(request, Fields{{"breakpoints", JsonValue::array()}});
    } else if (command == "configurationDone") {
        respond(request);
        std::lock_guard<std::mutex> lock(mutex_);
        configured_ = true;
        wake_.notify_all();
    } else if (command == "threads") {
        respond(request, Fields{{"threads", std::vector<JsonValue>{Fields{{"id", 1}, {"name", "FoxLang"}}}}});
    } else if (command == "pause") {
        pauseRequested_ = true;
        respond(request);
    } else if (command == "continue") {
        resume(request, Step::None);
    } else if (command == "next") {
        resume(request, Step::Over);
    } else if (command == "stepIn") {
        resume(request, Step::In);
    } else if (command == "stepOut") {
        resume(request, Step::Out);
    } else if (command == "stackTrace") {
        inspect(request, [&] { return stackTrace(arguments); });
    } else if (command == "scopes") {
        inspect(request, [&] { return scopes(arguments); });
    } else if (command == "variables") {
        inspect(request, [&] { return variables(arguments); });
    } else if (command == "setVariable") {
        inspect(request, [&] { return setVariable(arguments); });
    } else if (command == "evaluate") {
        inspect(request, [&] { return evaluateRequest(arguments); });
    } else if (command == "exceptionInfo") {
        inspect(request, [&] {
            return JsonValue(Fields{{"exceptionId", "Ошибка выполнения"}, {"description", exceptionText_}, {"breakMode", "always"}});
        });
    } else if (command == "source") {
        std::string identity;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto found = sourceReferences_.find(arguments["sourceReference"].asInt());
            if (found != sourceReferences_.end()) identity = found->second;
        }
        if (identity.empty() || !interpreter_) {
            respond(request, JsonValue(), "Исходный текст недоступен");
            return;
        }
        try {
            respond(request, Fields{{"content", interpreter_->getSources().read(identity)}});
        } catch (const std::exception& error) {
            respond(request, JsonValue(), error.what());
        }
    } else if (command == "disconnect" || command == "terminate") {
        respond(request);
        finish(0);
    } else {
        respond(request, JsonValue(), "Запрос " + command + " не поддерживается");
    }
}

void Session::setBreakpoints(const JsonValue& request) {
    const JsonValue& arguments = request["arguments"];
    std::string path = arguments["source"]["path"].asString();
    std::set<int> lines;
    std::string problem;
    if (!path.empty()) {
        try {
            std::ifstream file(platform::pathFromUtf8(path), std::ios::binary);
            std::stringstream text;
            text << file.rdbuf();
            Lexer lexer(text.str());
            Parser parser(lexer.tokenize(), path);
            statementLines(parser.parseProgram().get(), lines);
        } catch (const std::exception& error) {
            problem = std::string("Файл не разобран: ") + error.what();
        }
    }
    std::vector<Breakpoint> placed;
    std::vector<JsonValue> answer;
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& wanted : arguments["breakpoints"].asArray()) {
        Breakpoint breakpoint;
        breakpoint.id = nextBreakpointId_++;
        breakpoint.condition = trimmed(wanted["condition"].asString());
        breakpoint.hitCondition = trimmed(wanted["hitCondition"].asString());
        breakpoint.logMessage = wanted["logMessage"].asString();
        int line = wanted["line"].asInt();
        // A breakpoint on a blank line or a comment moves to the next statement.
        auto next = lines.lower_bound(line);
        JsonValue reply = Fields{{"id", breakpoint.id}};
        if (next == lines.end()) {
            reply["verified"] = false;
            reply["line"] = JsonValue(line);
            reply["message"] = problem.empty() ? std::string("Здесь и ниже нет исполняемого кода") : problem;
        } else {
            breakpoint.line = *next;
            reply["verified"] = true;
            reply["line"] = JsonValue(*next);
            placed.push_back(breakpoint);
        }
        answer.push_back(reply);
    }
    std::string key = fileKey(path);
    if (placed.empty()) {
        breakpoints_.erase(key);
    } else {
        breakpoints_[key] = std::move(placed);
    }
    anyBreakpoints_ = !breakpoints_.empty();
    respond(request, Fields{{"breakpoints", answer}});
}

void Session::resume(const JsonValue& request, Step step) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!paused_) {
        lock.unlock();
        respond(request, JsonValue(), "Программа не остановлена");
        return;
    }
    // The answer goes out before the program can stop again and say so.
    respond(request, step == Step::None ? JsonValue(Fields{{"allThreadsContinued", true}}) : JsonValue());
    step_ = step;
    resume_ = true;
    wake_.notify_all();
}

void Session::inspect(const JsonValue& request, const std::function<JsonValue()>& work) {
    std::promise<void> done;
    JsonValue body;
    std::string failure;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!paused_) {
            failure = "Программа выполняется: поставьте её на паузу, чтобы смотреть значения";
        } else {
            tasks_.push_back([&] {
                try {
                    body = work();
                } catch (const std::exception& error) {
                    failure = error.what();
                }
                done.set_value();
            });
            wake_.notify_all();
        }
    }
    if (failure.empty()) done.get_future().wait();
    respond(request, failure.empty() ? body : JsonValue(), failure);
}

int Session::serve() {
    std::thread(&Session::readRequests, this).detach();
    JsonValue launch;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        wake_.wait(lock, [&] { return launched_ && configured_; });
        launch = launch_;
    }

    std::error_code ignored;
    std::string cwd = launch["cwd"].asString();
    if (!cwd.empty()) std::filesystem::current_path(platform::pathFromUtf8(cwd), ignored);
    InterpreterOptions options;
    for (const auto& argument : launch["args"].asArray()) options.arguments.push_back(argument.asString());
    interpreter_ = std::make_unique<Interpreter>(options);
    interpreterThread_ = std::this_thread::get_id();
    frames_.push_back({programFrameName, nullptr, 0, &interpreter_->getContext()});
    if (launch["stopOnEntry"].asBool()) {
        step_ = Step::In;
        entry_ = true;
    }

    runtime::setDebugHook(this);
    RunResult result = interpreter_->runFile(launch["program"].asString());
    runtime::setDebugHook(nullptr);
    std::cout.flush();
    std::fflush(stdout);

    if (!result.errorMessage.empty()) {
        if (standardStreams_) {
            sendOutput("FoxLang: " + result.errorMessage + "\n", "stderr");
        } else {
            std::cerr << "FoxLang: " << result.errorMessage << std::endl;
        }
    }
    // Output captured from the program may still be on its way to the editor.
    if (standardStreams_) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    sendEvent("exited", Fields{{"exitCode", result.exitCode}});
    sendEvent("terminated");
    // The editor answers with disconnect, which ends the process.
    std::unique_lock<std::mutex> lock(mutex_);
    wake_.wait(lock, [] { return false; });
    return result.exitCode;
}

// ---------------------------------------------------------------- run control

void Session::statement(const std::string* file, int line) {
    if (evaluating_ || !onInterpreter()) return;
    Frame& frame = frames_.back();
    frame.file = file;
    frame.line = line;
    errorReported_ = false;

    std::string reason;
    if (pauseRequested_.exchange(false)) reason = "pause";
    else if (step_ == Step::In) reason = entry_ ? "entry" : "step";
    else if (step_ == Step::Over && frames_.size() <= stepDepth_) reason = "step";
    else if (step_ == Step::Out && frames_.size() < stepDepth_) reason = "step";

    std::vector<int> hitIds;
    if (reason.empty() && anyBreakpoints_ && breakpointAt(file, line, hitIds)) reason = "breakpoint";
    if (!reason.empty()) stop(reason, "", hitIds);
}

bool Session::breakpointAt(const std::string* file, int line, std::vector<int>& hitIds) {
    if (!file) return false;
    auto known = fileKeys_.find(file);
    if (known == fileKeys_.end()) known = fileKeys_.emplace(file, fileKey(*file)).first;
    Breakpoint copy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto list = breakpoints_.find(known->second);
        if (list == breakpoints_.end()) return false;
        auto found = std::find_if(list->second.begin(), list->second.end(), [&](const Breakpoint& b) { return b.line == line; });
        if (found == list->second.end()) return false;
        if (!accepts(*found)) return false;
        copy = *found;
    }
    if (!copy.logMessage.empty()) {
        // A logpoint prints and lets the program run on.
        sendOutput(interpolate(copy.logMessage, *frames_.back().scope) + "\n", "console");
        return false;
    }
    hitIds.push_back(copy.id);
    return true;
}

// Called with the breakpoint list locked; conditions run the program's own code.
bool Session::accepts(Breakpoint& breakpoint) {
    if (!breakpoint.condition.empty()) {
        try {
            Value result = evaluate(breakpoint.condition, *frames_.back().scope, false);
            if (result.type != "bool") throw std::runtime_error("условие должно быть bool, получено '" + result.type + "'");
            if (result.value != "true") return false;
        } catch (const std::exception& error) {
            sendOutput("Условие точки останова «" + breakpoint.condition + "»: " + error.what() + "\n", "stderr");
            return true;
        }
    }
    ++breakpoint.hits;
    if (breakpoint.hitCondition.empty()) return true;
    std::string text = breakpoint.hitCondition;
    std::string op = "==";
    for (const char* candidate : {">=", "<=", "==", ">", "<", "%"}) {
        if (text.rfind(candidate, 0) == 0) {
            op = candidate;
            text = trimmed(text.substr(op.size()));
            break;
        }
    }
    long long target = 0;
    try {
        target = std::stoll(text);
    } catch (const std::exception&) {
        return true;
    }
    long long hits = breakpoint.hits;
    if (op == ">=") return hits >= target;
    if (op == "<=") return hits <= target;
    if (op == ">") return hits > target;
    if (op == "<") return hits < target;
    if (op == "%") return target > 0 && hits % target == 0;
    return hits == target;
}

void Session::stop(const std::string& reason, const std::string& text, const std::vector<int>& hitIds) {
    step_ = Step::None;
    entry_ = false;
    references_.clear();
    std::cout.flush();
    std::fflush(stdout);
    JsonValue body = Fields{{"reason", reason}, {"threadId", 1}, {"allThreadsStopped", true}};
    if (!text.empty()) {
        body["description"] = "Ошибка выполнения";
        body["text"] = text;
    }
    if (!hitIds.empty()) {
        std::vector<JsonValue> ids;
        for (int id : hitIds) ids.emplace_back(id);
        body["hitBreakpointIds"] = ids;
    }
    std::unique_lock<std::mutex> lock(mutex_);
    paused_ = true;
    resume_ = false;
    sendEvent("stopped", body);
    for (;;) {
        wake_.wait(lock, [&] { return resume_ || !tasks_.empty(); });
        while (!tasks_.empty()) {
            auto task = std::move(tasks_.front());
            tasks_.pop_front();
            lock.unlock();
            task();
            lock.lock();
        }
        if (resume_) break;
    }
    paused_ = false;
    resume_ = false;
    stepDepth_ = frames_.size();
    references_.clear();
}

void Session::enterScope(Context& scope) {
    if (!onInterpreter()) return;
    outerBlocks_.push_back(frames_.back());
    frames_.back().scope = &scope;
}

void Session::leaveScope() {
    if (!onInterpreter() || outerBlocks_.empty()) return;
    // Blocks nest inside calls, so the frame on top is the one the block belonged to.
    Frame outer = std::move(outerBlocks_.back());
    outerBlocks_.pop_back();
    frames_.back().scope = outer.scope;
    frames_.back().file = outer.file;
    frames_.back().line = outer.line;
}

void Session::enterFunction(const std::string& name, const std::string* file, int line, Context& scope) {
    if (!onInterpreter()) return;
    frames_.push_back({name, file, line, &scope});
}

void Session::leaveFunction() {
    if (!onInterpreter() || frames_.size() <= 1) return;
    frames_.pop_back();
}

void Session::error(const std::string& message, bool caught) {
    if (evaluating_ || !onInterpreter() || errorReported_) return;
    // The same error passes every enclosing block on its way out: one stop is enough.
    errorReported_ = true;
    if (!(caught ? breakOnCaught_ : breakOnUncaught_)) return;
    exceptionText_ = message;
    stop("exception", message, {});
}

// ---------------------------------------------------------------- inspection

Value Session::evaluate(const std::string& text, Context& scope, bool statements) {
    // The program's own code runs here: it must not stop, and the location of the
    // statement the program is paused at stays what it was.
    struct Mark {
        bool& flag;
        runtime::StackGuard& guard;
        int line;
        const std::string* file;
        ~Mark() {
            flag = false;
            guard.line = line;
            guard.file = file;
        }
    };
    runtime::StackGuard& guard = runtime::stackGuard();
    Mark mark{evaluating_, guard, guard.line, guard.file};
    evaluating_ = true;
    try {
        try {
            Lexer lexer(text);
            Parser parser(lexer.tokenize(), "<консоль>");
            auto expression = parser.parseExpression();
            return expression->eval(scope);
        } catch (const SyntaxError&) {
            if (!statements) throw;
        }
        std::string source = trimmed(text);
        if (!source.empty() && source.back() != ';' && source.back() != '}') source += ';';
        Lexer lexer(source);
        Parser parser(lexer.tokenize(), "<консоль>");
        auto program = parser.parseProgram();
        for (auto& stmt : program->stmts) {
            if (stmt) stmt->eval(scope);
            if (guard.flow != runtime::StackGuard::Flow::None) {
                guard.flow = runtime::StackGuard::Flow::None;
                guard.returned = Value();
                throw std::runtime_error("return, break и continue в консоли отладчика не выполняются");
            }
        }
        return {"void", ""};
    } catch (const ExitRequest&) {
        throw std::runtime_error("exit() в консоли отладчика не выполняется: остановите отладку");
    }
}

std::string Session::interpolate(const std::string& message, Context& scope) {
    std::string out;
    for (size_t i = 0; i < message.size(); ++i) {
        if (message[i] != '{') {
            out += message[i];
            continue;
        }
        size_t close = message.find('}', i);
        if (close == std::string::npos) {
            out += message.substr(i);
            break;
        }
        std::string expression = message.substr(i + 1, close - i - 1);
        try {
            Value value = evaluate(expression, scope, false);
            out += value.type == "string" ? value.value.str() : preview(value, 0);
        } catch (const std::exception& error) {
            out += std::string("<") + error.what() + ">";
        }
        i = close;
    }
    return out;
}

std::string Session::preview(const Value& value, int depth) {
    if (value.type == "string") return quoted(value.value.str(), 500);
    if (value.type == "void") return "";
    if (!value.ref) return value.value.str();
    const Object& object = *value.ref;
    if (depth > 1) return object.kind == Object::Kind::Array ? "[…]" : "{…}";
    bool isArray = object.kind == Object::Kind::Array;
    bool isStruct = object.kind == Object::Kind::Struct;
    std::string out = isArray ? "[" : isStruct ? object.structType->name + "{" : "{";
    for (size_t i = 0; i < object.items.size(); ++i) {
        if (i == 8) {
            out += ", …";
            break;
        }
        if (i > 0) out += ", ";
        if (!isArray) out += (isStruct ? object.structType->fields[i].name : object.keys[i]) + ": ";
        out += preview(object.items[i], depth + 1);
    }
    return out + (isArray ? "]" : "}");
}

int Session::referenceFor(Reference reference) {
    references_.push_back(std::move(reference));
    return static_cast<int>(references_.size());
}

JsonValue Session::variable(const std::string& name, const Value& value) {
    JsonValue out = Fields{{"name", name}, {"value", preview(value, 0)}, {"type", value.type.str()}, {"variablesReference", 0}};
    if (value.ref) {
        out["variablesReference"] = JsonValue(referenceFor({Reference::Kind::Container, 0, value.ref}));
        size_t size = value.ref->items.size();
        if (value.ref->kind == Object::Kind::Array) {
            out["indexedVariables"] = JsonValue(size);
            out["type"] = "array (" + std::to_string(size) + ")";
        } else {
            out["namedVariables"] = JsonValue(size);
            if (value.ref->kind == Object::Kind::Map) out["type"] = "map (" + std::to_string(size) + ")";
        }
    }
    return out;
}

// The name a child of a container is shown under: [i], a map key or a field.
std::string childName(const Object& object, size_t index) {
    if (object.kind == Object::Kind::Array) return "[" + std::to_string(index) + "]";
    if (object.kind == Object::Kind::Map) return quoted(object.keys[index], 200);
    return object.structType->fields[index].name;
}

JsonValue Session::source(const std::string* file) {
    if (!file) return JsonValue();
    const std::string& identity = *file;
    if (identity.rfind("@", 0) == 0) {
        // Built-in modules have no file on disk; the editor asks for their text.
        std::lock_guard<std::mutex> lock(mutex_);
        int reference = 0;
        for (const auto& entry : sourceReferences_)
            if (entry.second == identity) reference = entry.first;
        if (!reference) {
            reference = static_cast<int>(sourceReferences_.size()) + 1;
            sourceReferences_[reference] = identity;
        }
        return Fields{{"name", runtime::displayPath(identity)}, {"sourceReference", reference}, {"presentationHint", "deemphasize"}};
    }
    std::string name = platform::pathToUtf8(platform::pathFromUtf8(identity).filename());
    return Fields{{"name", name}, {"path", identity}};
}

JsonValue Session::stackTrace(const JsonValue& arguments) {
    size_t start = static_cast<size_t>(std::max(0, arguments["startFrame"].asInt()));
    size_t levels = static_cast<size_t>(std::max(0, arguments["levels"].asInt()));
    std::vector<JsonValue> list;
    for (size_t shown = 0, i = frames_.size(); i-- > 0; ++shown) {
        if (shown < start) continue;
        if (levels && list.size() >= levels) break;
        const Frame& frame = frames_[i];
        JsonValue item = Fields{{"id", static_cast<int>(i) + 1}, {"name", frame.name}, {"line", std::max(1, frame.line)}, {"column", 1}};
        JsonValue where = source(frame.file);
        if (!where.isNull()) item["source"] = where;
        list.push_back(item);
    }
    return Fields{{"stackFrames", list}, {"totalFrames", frames_.size()}};
}

JsonValue Session::scopes(const JsonValue& arguments) {
    size_t frame = static_cast<size_t>(std::max(1, arguments["frameId"].asInt()) - 1);
    if (frame >= frames_.size()) throw std::runtime_error("Нет такого кадра стека");
    std::vector<JsonValue> list;
    list.push_back(Fields{{"name", "Локальные"}, {"presentationHint", "locals"}, {"expensive", false},
                          {"variablesReference", referenceFor({Reference::Kind::Locals, frame, nullptr})}});
    list.push_back(Fields{{"name", "Глобальные"}, {"expensive", false},
                          {"variablesReference", referenceFor({Reference::Kind::Globals, frame, nullptr})}});
    return Fields{{"scopes", list}};
}

JsonValue Session::variables(const JsonValue& arguments) {
    int id = arguments["variablesReference"].asInt();
    if (id < 1 || static_cast<size_t>(id) > references_.size()) throw std::runtime_error("Значения устарели: программа продолжила работу");
    Reference reference = references_[static_cast<size_t>(id) - 1];
    std::vector<JsonValue> list;
    if (reference.kind == Reference::Kind::Container) {
        const Object& object = *reference.container;
        size_t size = object.items.size();
        size_t start = static_cast<size_t>(std::max(0, arguments["start"].asInt()));
        size_t count = arguments.has("count") ? static_cast<size_t>(std::max(0, arguments["count"].asInt())) : size;
        for (size_t i = start; i < size && i < start + count; ++i) {
            // The element is copied first: a child's reference may grow the list.
            Value item = object.items[i];
            list.push_back(variable(childName(object, i), item));
        }
    } else if (reference.kind == Reference::Kind::Globals) {
        for (const auto& entry : root().variables) list.push_back(variable(entry.first, entry.second));
    } else {
        // The innermost declaration hides outer ones of the same name, as in the program:
        // block scopes first, then the frame's slots from the latest declared, until the
        // frame that owns them. The program's own frame is the root's slots.
        std::set<std::string> seen;
        for (Context* scope = frames_[reference.frame].scope; scope; scope = scope->parent) {
            if (scope->parent)
                for (const auto& entry : scope->variables)
                    if (seen.insert(entry.first).second) list.push_back(variable(entry.first, entry.second));
            if (scope->frame && scope->slots && scope->slotNames) {
                const auto& names = *scope->slotNames;
                for (size_t i = names.size(); i-- > 0;) {
                    const Value& slot = scope->slots[i];
                    if (!slot.type.is(TypeName::Kind::Void) && seen.insert(names[i]).second) list.push_back(variable(names[i], slot));
                }
                break;
            }
        }
    }
    return Fields{{"variables", list}};
}

JsonValue Session::setVariable(const JsonValue& arguments) {
    int id = arguments["variablesReference"].asInt();
    if (id < 1 || static_cast<size_t>(id) > references_.size()) throw std::runtime_error("Значения устарели: программа продолжила работу");
    Reference reference = references_[static_cast<size_t>(id) - 1];
    std::string name = arguments["name"].asString();
    Context& scope = *frames_[reference.kind == Reference::Kind::Container ? frames_.size() - 1 : reference.frame].scope;
    Value value = evaluate(arguments["value"].asString(), scope, false);
    if (reference.kind == Reference::Kind::Container) {
        Object& object = *reference.container;
        size_t index = object.items.size();
        for (size_t i = 0; i < object.items.size(); ++i)
            if (childName(object, i) == name) index = i;
        if (index >= object.items.size()) throw std::runtime_error("Нет такого элемента");
        if (object.kind == Object::Kind::Struct) runtime::coerce(object.structType->fields[index].type, value, "поле '" + name + "'");
        object.items[index] = value;
        return variable(name, object.items[index]);
    }
    Value* target = nullptr;
    if (reference.kind == Reference::Kind::Globals) {
        auto found = root().variables.find(name);
        if (found != root().variables.end()) target = &found->second;
    } else {
        target = scope.findVar(name);
    }
    if (!target) throw std::runtime_error("Переменная '" + name + "' не найдена");
    runtime::assign(*target, value, name);
    return variable(name, *target);
}

JsonValue Session::evaluateRequest(const JsonValue& arguments) {
    int frameId = arguments["frameId"].asInt();
    size_t frame = frameId >= 1 && static_cast<size_t>(frameId) <= frames_.size() ? static_cast<size_t>(frameId) - 1 : frames_.size() - 1;
    bool console = arguments["context"].asString() == "repl";
    Value value = evaluate(arguments["expression"].asString(), *frames_[frame].scope, console);
    JsonValue shown = variable("", value);
    JsonValue body = Fields{{"result", shown["value"]}, {"type", shown["type"]}, {"variablesReference", shown["variablesReference"]}};
    if (shown.has("indexedVariables")) body["indexedVariables"] = shown["indexedVariables"];
    return body;
}

} // namespace

int runAdapter(const std::vector<std::string>& options) {
    std::string target;
    for (size_t i = 0; i < options.size(); ++i) {
        if (options[i] == "--connect" && i + 1 < options.size()) {
            target = options[++i];
        } else {
            throw std::runtime_error("Usage: foxlang debug-adapter [--connect host:port]");
        }
    }
    Session session(target.empty());
    if (target.empty()) {
        session.attach(Channel::standardStreams([&session](const std::string& text) { session.sendOutput(text, "stdout"); }));
    } else {
        size_t colon = target.rfind(':');
        if (colon == std::string::npos) throw std::runtime_error("Debug Error: expected host:port, got " + target);
        session.attach(Channel::connect(target.substr(0, colon), std::stoi(target.substr(colon + 1))));
    }
    return session.serve();
}

} // namespace foxlang::debug
