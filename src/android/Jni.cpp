#include "foxlang/Android.h"
#include "foxlang/FoxLang.h"
#include "NetworkLicenses.h"
#include <jni.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <codecvt>
#include <condition_variable>
#include <locale>
#include <map>
#include <mutex>
#include <sstream>

namespace {
using namespace foxlang;
std::mutex runtimeMutex, handlesMutex;
struct Session {
    std::unique_ptr<Interpreter> interpreter;
    std::string directory;
    std::atomic<jlong> epoch{0};
    std::mutex eventsMutex;
    std::condition_variable wake;
    std::vector<android::Event> events;
};
std::map<jlong, std::shared_ptr<Session>> sessions;
jlong nextHandle = 1;
std::string string(JNIEnv* env, jstring value) {
    if (!value) throw std::invalid_argument("null string");
    const jchar* chars = env->GetStringChars(value, nullptr);
    if (!chars) throw std::runtime_error("cannot read Java string");
    std::u16string copy(reinterpret_cast<const char16_t*>(chars), env->GetStringLength(value));
    env->ReleaseStringChars(value, chars);
    return std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}.to_bytes(copy);
}
jstring string(JNIEnv* env, const std::string& value) {
    auto copy = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}.from_bytes(value);
    return env->NewString(reinterpret_cast<const jchar*>(copy.data()), static_cast<jsize>(copy.size()));
}
std::shared_ptr<Session> session(jlong handle) {
    std::lock_guard<std::mutex> lock(handlesMutex);
    auto found = sessions.find(handle);
    if (found == sessions.end()) throw std::runtime_error("FoxLang engine is closed");
    return found->second;
}
void failure(JNIEnv* env, const std::exception& error) {
    if (!env->ExceptionCheck()) env->ThrowNew(env->FindClass("java/lang/IllegalStateException"), error.what());
}
struct Host final : android::Host {
    Session& state;
    JNIEnv* env;
    jobject callbacks;
    jclass type;
    jlong epoch;
    std::istringstream stdinText;
    std::string captured;
    Host(Session& s, JNIEnv* e, jobject c, jlong generation, std::string input)
        : state(s), env(e), callbacks(c), type(e->GetObjectClass(c)), epoch(generation), stdinText(std::move(input)) {}
    ~Host() override { env->DeleteLocalRef(type); }
    jmethodID method(const char* name, const char* sig) {
        auto m = env->GetMethodID(type, name, sig);
        if (!m) { env->ExceptionClear(); throw std::runtime_error(std::string("Missing Android Host method: ") + name); }
        return m;
    }
    void checked() {
        if (env->ExceptionCheck()) { env->ExceptionClear(); throw std::runtime_error("Android Host callback failed"); }
    }
    std::string directory() const override { return state.directory; }
    void checkCancelled() override { if (state.epoch.load() != epoch) throw ExitRequest{130}; }
    void pause(long long milliseconds) override {
        checkCancelled();
        std::unique_lock<std::mutex> lock(state.eventsMutex);
        state.wake.wait_for(lock, std::chrono::milliseconds(std::max(0LL, milliseconds)), [&] { return state.epoch.load() != epoch; });
        checkCancelled();
    }
    void output(const std::string& value, bool error) override {
        if (captured.size() < 1000000) {
            size_t length = std::min(value.size(), 1000000 - captured.size());
            while (length < value.size() && length > 0 && (static_cast<unsigned char>(value[length]) & 0xc0) == 0x80) --length;
            captured.append(value, 0, length);
        }
        auto text = string(env, value);
        env->CallVoidMethod(callbacks, method("onOutput", "(Ljava/lang/String;Z)V"), text, error);
        env->DeleteLocalRef(text); checked(); checkCancelled();
    }
    std::string input() override { checkCancelled(); std::string line; std::getline(stdinText, line); if (!line.empty() && line.back() == '\r') line.pop_back(); return line; }
    void open(int w, int h, const std::string& title) override {
        auto text = string(env, title);
        env->CallVoidMethod(callbacks, method("onOpen", "(IILjava/lang/String;)V"), w, h, text);
        env->DeleteLocalRef(text); checked();
    }
    void close() noexcept override {
        try { env->CallVoidMethod(callbacks, method("onClose", "()V")); checked(); } catch (...) {}
    }
    void present(const uint32_t* pixels, int w, int h) override {
        checkCancelled();
        auto frame = env->NewIntArray(w * h);
        if (!frame) { env->ExceptionClear(); throw std::runtime_error("Android frame allocation failed"); }
        std::vector<jint> argb(static_cast<size_t>(w) * h);
        for (size_t i = 0; i < argb.size(); ++i) argb[i] = static_cast<jint>(0xff000000u | pixels[i]);
        env->SetIntArrayRegion(frame, 0, w * h, argb.data());
        env->CallVoidMethod(callbacks, method("onFrame", "([III)V"), frame, w, h);
        env->DeleteLocalRef(frame); checked();
    }
    std::vector<android::Event> poll() override {
        pause(16); // Yield to Android; a tight GUI loop must not starve the UI thread.
        std::lock_guard<std::mutex> lock(state.eventsMutex);
        std::vector<android::Event> result;
        result.swap(state.events);
        return result;
    }
    void resizable(bool on) override { env->CallVoidMethod(callbacks, method("onResizable", "(Z)V"), on); checked(); }
    void fullscreen(bool on) override { env->CallVoidMethod(callbacks, method("onFullscreen", "(Z)V"), on); checked(); }
    std::string clipboard() override {
        auto text = static_cast<jstring>(env->CallObjectMethod(callbacks, method("getClipboard", "()Ljava/lang/String;")));
        checked(); auto result = text ? string(env, text) : ""; if (text) env->DeleteLocalRef(text); return result;
    }
    void clipboard(const std::string& value) override {
        auto text = string(env, value);
        env->CallVoidMethod(callbacks, method("setClipboard", "(Ljava/lang/String;)V"), text);
        env->DeleteLocalRef(text); checked();
    }
    bool playWav(const std::vector<uint8_t>& value) override {
        auto bytes = env->NewByteArray(static_cast<jsize>(value.size()));
        if (!bytes) { env->ExceptionClear(); return false; }
        env->SetByteArrayRegion(bytes, 0, static_cast<jsize>(value.size()), reinterpret_cast<const jbyte*>(value.data()));
        bool result = env->CallBooleanMethod(callbacks, method("playWav", "([B)Z"), bytes);
        env->DeleteLocalRef(bytes); checked(); return result;
    }
    void stopSound() noexcept override {
        try { env->CallVoidMethod(callbacks, method("stopSound", "()V")); checked(); } catch (...) {}
    }
    bool openUri(const std::string& uri) override {
        auto text = string(env, uri);
        bool result = env->CallBooleanMethod(callbacks, method("openUri", "(Ljava/lang/String;)Z"), text);
        env->DeleteLocalRef(text); checked(); return result;
    }
};
jobjectArray array(JNIEnv* env, const std::vector<std::string>& values) {
    auto result = env->NewObjectArray(static_cast<jsize>(values.size()), env->FindClass("java/lang/String"), nullptr);
    if (!result) return nullptr;
    for (size_t i = 0; i < values.size(); ++i) { auto text = string(env, values[i]); env->SetObjectArrayElement(result, static_cast<jsize>(i), text); env->DeleteLocalRef(text); }
    return result;
}
}
#define JNI_METHOD(name) Java_org_foxlang_android_FoxLang_##name
extern "C" JNIEXPORT jstring JNICALL JNI_METHOD(nativeVersion)(JNIEnv* env, jclass) {
    return string(env, foxlang::Interpreter::getVersion());
}
extern "C" JNIEXPORT jstring JNICALL JNI_METHOD(nativeNotices)(JNIEnv* env, jclass) {
    try { return string(env, networkLicenses()); }
    catch (const std::exception& e) { failure(env, e); return nullptr; }
}
extern "C" JNIEXPORT jlong JNICALL JNI_METHOD(nativeCreate)(JNIEnv* env, jclass, jstring directory, jobjectArray args) {
    try {
        std::lock_guard<std::mutex> execution(runtimeMutex);
        auto s = std::make_shared<Session>(); s->directory = string(env, directory);
        InterpreterOptions options; options.loadDotEnv = false; options.foxHome = s->directory;
        for (jsize i = 0; i < env->GetArrayLength(args); ++i) { auto arg = static_cast<jstring>(env->GetObjectArrayElement(args, i)); options.arguments.push_back(string(env, arg)); env->DeleteLocalRef(arg); }
        s->interpreter = std::make_unique<Interpreter>(std::move(options));
        std::lock_guard<std::mutex> lock(handlesMutex); jlong id = nextHandle++; sessions[id] = s; return id;
    } catch (const std::exception& e) { failure(env, e); return 0; }
}
extern "C" JNIEXPORT jobjectArray JNICALL JNI_METHOD(nativeRun)(JNIEnv* env, jclass, jlong handle, jstring source, jstring path, jstring input, jboolean file, jobject callbacks, jlong epoch) {
    try {
        std::lock_guard<std::mutex> execution(runtimeMutex);
        auto s = session(handle);
        { std::lock_guard<std::mutex> lock(s->eventsMutex); s->events.clear(); }
        Host host(*s, env, callbacks, epoch, string(env, input)); android::Scope scope(host);
        RunResult result;
        if (s->epoch.load() != epoch) result = {false, 130, ""};
        else if (file) result = s->interpreter->runFile(string(env, path));
        else result = s->interpreter->runSource(string(env, source), string(env, path));
        s->interpreter->getContext().graphics.reset(); // Window callbacks cannot outlive the JNI call.
        host.stopSound();
        return array(env, {result.success ? "true" : "false", std::to_string(result.exitCode), result.errorMessage, host.captured});
    } catch (const std::exception& e) { failure(env, e); return nullptr; }
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(nativeCancel)(JNIEnv* env, jclass, jlong handle, jlong epoch) {
    try { auto s = session(handle); s->epoch.store(epoch); s->wake.notify_all(); } catch (const std::exception& e) { failure(env, e); }
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(nativeEvent)(JNIEnv* env, jclass, jlong handle, jint kind, jint a, jint b, jstring text) {
    try {
        if (kind < 0 || kind > 6) throw std::invalid_argument("invalid input event");
        if (kind == 5 && (a < 64 || a > 4096 || b < 64 || b > 4096)) return;
        auto s = session(handle); android::Event e{static_cast<android::Event::Kind>(kind), a, b, string(env, text)};
        std::lock_guard<std::mutex> lock(s->eventsMutex);
        // Coalesce pointer motion/resize; preserve key edges and committed text.
        if (!s->events.empty() && (kind == 2 || kind == 5) && s->events.back().kind == kind) s->events.back() = std::move(e);
        else {
            if (s->events.size() >= 4096) {
                // A flooded queue must not leave a dropped key-up held forever.
                s->events.clear();
                s->events.push_back({android::Event::Focus, 0, 0, ""});
                s->events.push_back({android::Event::Focus, 1, 0, ""});
            }
            s->events.push_back(std::move(e));
        }
    } catch (const std::exception& e) { failure(env, e); }
}
extern "C" JNIEXPORT jobjectArray JNICALL JNI_METHOD(nativeGet)(JNIEnv* env, jclass, jlong handle, jstring name) {
    try { std::lock_guard<std::mutex> execution(runtimeMutex); auto value = session(handle)->interpreter->getGlobal(string(env, name)); return array(env, {value.typeName(), runtime::display(value)}); }
    catch (const std::exception& e) { failure(env, e); return nullptr; }
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(nativeSet)(JNIEnv* env, jclass, jlong handle, jstring name, jstring type, jstring value) {
    try { std::lock_guard<std::mutex> execution(runtimeMutex); session(handle)->interpreter->setGlobal(string(env, name), string(env, type), string(env, value)); }
    catch (const std::exception& e) { failure(env, e); }
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(nativeReset)(JNIEnv* env, jclass, jlong handle) {
    try { std::lock_guard<std::mutex> execution(runtimeMutex); session(handle)->interpreter->reset(); }
    catch (const std::exception& e) { failure(env, e); }
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(nativeClose)(JNIEnv*, jclass, jlong handle) {
    std::lock_guard<std::mutex> execution(runtimeMutex);
    std::lock_guard<std::mutex> lock(handlesMutex); sessions.erase(handle);
}
