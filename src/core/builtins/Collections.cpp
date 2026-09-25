// Array algorithms that are better done natively than in a FoxLang loop.
#include "Builtin.h"
#include <algorithm>
#include <stdexcept>

namespace foxlang::runtime {
namespace {

bool isNumber(const Value& value) { return value.isNumber(); }

// Numbers compare by value, everything else by text, so a FoxLang wrapper that
// passes the needle as a string still finds the number 5 when asked for "5".
bool sameValue(const Value& a, const Value& b) {
    if (isNumber(a) && isNumber(b)) {
        double x = 0, y = 0;
        return tryNumber(a, x) && tryNumber(b, y) && x == y;
    }
    if (a.ref() || b.ref()) return deepEqual(a, b);
    if (a.isString() && b.isString()) return a.str() == b.str();
    return a.text() == b.text();
}

size_t bound(Call& call, size_t argument, size_t size) {
    long long value = call.integer(argument);
    if (value < 0) value += static_cast<long long>(size);
    return static_cast<size_t>(std::clamp<long long>(value, 0, static_cast<long long>(size)));
}

} // namespace

void addCollectionBuiltins(std::vector<Builtin>& out) {
    auto add = [&](BuiltinSpec spec, Handler handler) { out.push_back({std::move(spec), handler}); };

    add({"array_sort", "void", {{"array", "items"}}, 1, false, "arrays",
         "Сортирует массив по возрастанию на месте: числа по значению, строки по алфавиту (по байтам UTF-8). "
         "Массив из чисел и строк вперемешку — ошибка выполнения."},
        [](Call& c) {
            auto& items = c.array(0);
            bool numbers = std::all_of(items.begin(), items.end(), isNumber);
            bool strings = std::all_of(items.begin(), items.end(), [](const Value& v) { return v.isString(); });
            if (numbers) {
                std::stable_sort(items.begin(), items.end(), [](const Value& a, const Value& b) {
                    double x = 0, y = 0;
                    tryNumber(a, x);
                    tryNumber(b, y);
                    return x < y;
                });
            } else if (strings) {
                std::stable_sort(items.begin(), items.end(), [](const Value& a, const Value& b) { return a.str() < b.str(); });
            } else {
                throw std::runtime_error("Runtime Error: array_sort() needs all numbers or all strings");
            }
            return nothing();
        });
    add({"array_reverse", "void", {{"array", "items"}}, 1, false, "arrays",
         "Переставляет элементы массива в обратном порядке на месте."},
        [](Call& c) {
            auto& items = c.array(0);
            std::reverse(items.begin(), items.end());
            return nothing();
        });
    add({"array_index_of", "int", {{"array", "items"}, {"any", "value"}}, 2, false, "arrays",
         "Индекс первого элемента, равного `value`, или `-1`. Числа сравниваются по величине (`1` равно `1.0`), "
         "остальные значения — по тексту (`5` равно `\"5\"`)."},
        [](Call& c) {
            const auto& items = c.array(0);
            for (size_t i = 0; i < items.size(); ++i)
                if (sameValue(items[i], c.at(1))) return integer(static_cast<long long>(i));
            return integer(-1);
        });
    add({"array_copy", "array", {{"array", "items"}}, 1, false, "arrays",
         "Новый массив с теми же элементами; изменения копии не затрагивают исходный массив."},
        [](Call& c) { return deepCopy(c.at(0)); });
    add({"array_slice", "array", {{"array", "items"}, {"int", "start"}, {"int", "end"}}, 2, false, "arrays",
         "Новый массив из элементов с `start` до `end` (не включая). Без `end` — до конца; "
         "отрицательные индексы отсчитываются с конца."},
        [](Call& c) {
            const auto& items = c.array(0);
            size_t from = bound(c, 1, items.size());
            size_t to = c.has(2) ? bound(c, 2, items.size()) : items.size();
            std::vector<Value> part;
            for (size_t i = from; i < to; ++i) part.push_back(items[i]);
            return makeArray(std::move(part));
        });
}

} // namespace foxlang::runtime
