#include "foxlang/Android.h"
#include <stdexcept>
namespace foxlang::android {
namespace { thread_local Host* active = nullptr; }
Host* current() noexcept { return active; }
Host& host() {
    if (!active) throw std::runtime_error("Android Error: this operation requires an Android Host");
    return *active;
}
Scope::Scope(Host& value) : previous(active) { active = &value; }
Scope::~Scope() { active = previous; }
void checkpoint() { if (active) active->checkCancelled(); }
}
