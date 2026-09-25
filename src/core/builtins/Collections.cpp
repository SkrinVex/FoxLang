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

// Calls a function argument with one or two values. The items are copied first: the
// function may change the array it walks.
Value apply(Call& call, const Value& function, const Value& a) {
    Value args[1] = {a};
    return callValue(function, args, 1, call.ctx);
}

Value apply(Call& call, const Value& function, const Value& a, const Value& b) {
    Value args[2] = {a, b};
    return callValue(function, args, 2, call.ctx);
}

bool test(Call& call, const Value& function, const Value& item) {
    Value result = apply(call, function, item);
    if (!result.isBool())
        throw std::runtime_error("Type Error: the function passed to " + call.spec.name + "() must return bool, got '" +
                                 result.typeName() + "'");
    return result.asBool();
}

// A merge sort by a FoxLang function. Unlike std::sort it stays within the array even
// when the function does not order the items consistently.
void sortBy(Call& call, std::vector<Value>& items, const Value& before) {
    std::vector<Value> buffer(items.size());
    for (size_t width = 1; width < items.size(); width *= 2) {
        for (size_t left = 0; left < items.size(); left += 2 * width) {
            size_t middle = std::min(left + width, items.size());
            size_t right = std::min(left + 2 * width, items.size());
            size_t i = left, j = middle, k = left;
            while (i < middle && j < right) {
                // Stable: an item from the right half goes first only when it is strictly before.
                Value result = apply(call, before, items[j], items[i]);
                if (!result.isBool())
                    throw std::runtime_error("Type Error: the function passed to array_sort() must return bool, got '" +
                                             result.typeName() + "'");
                buffer[k++] = std::move(result.asBool() ? items[j++] : items[i++]);
            }
            while (i < middle) buffer[k++] = std::move(items[i++]);
            while (j < right) buffer[k++] = std::move(items[j++]);
        }
        items.swap(buffer);
    }
}

} // namespace

void addCollectionBuiltins(std::vector<Builtin>& out) {
    auto add = [&](BuiltinSpec spec, Handler handler) { out.push_back({std::move(spec), handler}); };

    add({"array_sort", "void", {{"array", "items"}, {"func", "before"}}, 1, false, "arrays",
         "Сортирует массив по возрастанию на месте: числа по значению, строки по алфавиту (по байтам UTF-8). "
         "Массив из чисел и строк вперемешку — ошибка выполнения. С функцией `before(a, b)`, которая "
         "возвращает `true`, когда `a` должен стоять раньше `b`, порядок задаёт она; равные элементы "
         "сохраняют исходный порядок."},
        [](Call& c) {
            if (c.has(1)) {
                std::vector<Value> sorted = c.array(0);
                sortBy(c, sorted, c.at(1));
                c.array(0) = std::move(sorted);
                return nothing();
            }
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
    add({"array_map", "array", {{"array", "items"}, {"func", "transform"}}, 2, false, "arrays",
         "Новый массив из результатов `transform(item)` для каждого элемента."},
        [](Call& c) {
            std::vector<Value> items = c.array(0);
            std::vector<Value> result;
            result.reserve(items.size());
            for (const auto& item : items) result.push_back(apply(c, c.at(1), item));
            return makeArray(std::move(result));
        });
    add({"array_filter", "array", {{"array", "items"}, {"func", "keep"}}, 2, false, "arrays",
         "Новый массив из элементов, для которых `keep(item)` вернула `true`."},
        [](Call& c) {
            std::vector<Value> items = c.array(0);
            std::vector<Value> result;
            for (auto& item : items)
                if (test(c, c.at(1), item)) result.push_back(std::move(item));
            return makeArray(std::move(result));
        });
    add({"array_reduce", "any", {{"array", "items"}, {"func", "combine"}, {"any", "initial"}}, 3, false, "arrays",
         "Сворачивает массив в одно значение: начинает с `initial` и для каждого элемента "
         "вызывает `combine(result, item)`."},
        [](Call& c) {
            std::vector<Value> items = c.array(0);
            Value result = c.at(2);
            for (const auto& item : items) result = apply(c, c.at(1), result, item);
            return result;
        });
    add({"array_find", "int", {{"array", "items"}, {"func", "matches"}}, 2, false, "arrays",
         "Индекс первого элемента, для которого `matches(item)` вернула `true`, или `-1`."},
        [](Call& c) {
            std::vector<Value> items = c.array(0);
            for (size_t i = 0; i < items.size(); ++i)
                if (test(c, c.at(1), items[i])) return integer(static_cast<long long>(i));
            return integer(-1);
        });
    add({"array_any", "bool", {{"array", "items"}, {"func", "matches"}}, 2, false, "arrays",
         "`true`, если `matches(item)` вернула `true` хотя бы для одного элемента."},
        [](Call& c) {
            std::vector<Value> items = c.array(0);
            for (const auto& item : items)
                if (test(c, c.at(1), item)) return boolean(true);
            return boolean(false);
        });
    add({"array_all", "bool", {{"array", "items"}, {"func", "matches"}}, 2, false, "arrays",
         "`true`, если `matches(item)` вернула `true` для каждого элемента (и для пустого массива)."},
        [](Call& c) {
            std::vector<Value> items = c.array(0);
            for (const auto& item : items)
                if (!test(c, c.at(1), item)) return boolean(false);
            return boolean(true);
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
