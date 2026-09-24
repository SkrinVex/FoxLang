// Core builtins: output, input, arrays, conversions and math.
#include "Builtin.h"
#include "foxlang/Platform.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <random>
#include <stdexcept>
#include <thread>

namespace foxlang::runtime {
namespace {

constexpr size_t maxElements = size_t{1} << 26;

std::mt19937& generator() {
    static std::mt19937 engine{std::random_device{}()};
    return engine;
}

// Arrays print as [a, b, c]; their internal id means nothing to a reader.
std::string display(const Value& value, Context& ctx, int depth = 0) {
    if (value.type != "array") return value.value.str();
    if (depth > 8) return "[...]";
    auto& arrays = ctx.getRoot()->arrays;
    auto found = arrays.find(value.value.str());
    if (found == arrays.end()) return "[]";
    std::string out = "[";
    for (size_t i = 0; i < found->second.size(); ++i) {
        if (i > 0) out += ", ";
        out += display(found->second[i], ctx, depth + 1);
    }
    return out + "]";
}

size_t checkedIndex(Call& call, size_t argument, size_t size) {
    long long index = call.integer(argument);
    if (index < 0 || static_cast<unsigned long long>(index) >= size)
        throw std::runtime_error("Runtime Error: Array index out of bounds: " + std::to_string(index) +
                                 " (size " + std::to_string(size) + ")");
    return static_cast<size_t>(index);
}

std::string trimmed(const std::string& text) {
    auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

bool parseNumber(const std::string& raw, double& out) {
    std::string text = trimmed(raw);
    if (text.empty()) return false;
    Value probe{"string", text};
    return tryNumber(probe, out);
}

bool allIntegers(Call& call) {
    return std::all_of(call.args.begin(), call.args.end(), [](const Value& v) { return v.type == "int"; });
}

Value numberResult(Call& call, double result, const char* op) {
    if (allIntegers(call)) return {"int", intResult(static_cast<long long>(result), op)};
    return real(result);
}

Value unaryMath(Call& call, double (*function)(double)) {
    return real(function(call.number(0)));
}

} // namespace

void addCoreBuiltins(std::vector<Builtin>& out) {
    auto add = [&](BuiltinSpec spec, Handler handler) { out.push_back({std::move(spec), handler}); };

    // Input and output
    add({"print", "void", {{"any", "values"}}, 0, true, "",
         "Печатает значения через пробел и переводит строку. Массив выводится как `[1, 2, 3]`.\n\n"
         "```foxlang\nprint(\"Счёт:\", score);\n```"},
        [](Call& c) {
            for (size_t i = 0; i < c.count(); ++i) {
                if (i > 0) std::cout << ' ';
                std::cout << display(c.at(i), c.ctx);
            }
            std::cout << std::endl;
            return nothing();
        });
    add({"input", "string", {{"string", "prompt"}}, 0, false, "",
         "Читает строку из стандартного ввода. Необязательное приглашение печатается без перевода строки. "
         "В конце ввода возвращает пустую строку.\n\n```foxlang\nstring name = input(\"Имя: \");\n```"},
        [](Call& c) {
            if (c.has(0)) std::cout << c.text(0) << std::flush;
            std::string line;
            if (!std::getline(std::cin, line)) return text("");
            if (!line.empty() && line.back() == '\r') line.pop_back();
            return text(line);
        });
    add({"getch", "string", {}, 0, false, "",
         "Читает одну клавишу из терминала без ожидания Enter и без эха."},
        [](Call&) { return text(platform::getch()); });
    add({"kbhit", "bool", {}, 0, false, "",
         "Проверяет, есть ли нажатая клавиша в буфере терминала, не дожидаясь её."},
        [](Call&) { return boolean(platform::kbhit()); });
    add({"wait", "void", {{"int", "milliseconds"}}, 1, false, "",
         "Приостанавливает программу на заданное число миллисекунд.\n\n```foxlang\nwait(500);\n```"},
        [](Call& c) {
            std::this_thread::sleep_for(std::chrono::milliseconds(c.amount(0, 86400000)));
            return nothing();
        });
    add({"fail", "void", {{"string", "message"}}, 1, false, "",
         "Останавливает программу с ошибкой выполнения и сообщением, как любая другая ошибка: с файлом и строкой. "
         "В обработчике HTTP-запроса даёт ответ 500, а сервер продолжает работу.\n\n"
         "```foxlang\nif (price < 0) { fail(\"цена не может быть отрицательной\"); }\n```"},
        [](Call& c) -> Value { throw std::runtime_error("Runtime Error: " + c.text(0)); });
    add({"exit", "void", {{"int", "code"}}, 0, false, "",
         "Немедленно завершает программу с кодом возврата (по умолчанию `0`).\n\n```foxlang\nexit(2);\n```"},
        [](Call& c) -> Value { throw ExitRequest{c.has(0) ? static_cast<int>(c.integer(0)) : 0}; });

    // Arrays
    add({"size", "int", {{"any", "value"}}, 1, false, "",
         "Число элементов массива или длина строки в **байтах** UTF-8. Символы строки считает `str_length`.\n\n"
         "```foxlang\nint n = size(scores);\n```"},
        [](Call& c) {
            const Value& value = c.at(0);
            if (value.type == "string") return integer(static_cast<long long>(value.value.length()));
            if (value.type == "array") return integer(static_cast<long long>(c.array(0).size()));
            throw std::runtime_error("Type Error: size() requires an array or a string, got '" + value.type + "'");
        });
    add({"get", "any", {{"array", "items"}, {"int", "index"}}, 2, false, "",
         "Элемент массива по индексу (с нуля). То же, что `items[index]`.\n\n```foxlang\nint first = get(scores, 0);\n```"},
        [](Call& c) {
            auto& items = c.array(0);
            return items[checkedIndex(c, 1, items.size())];
        });
    add({"set", "void", {{"array", "items"}, {"int", "index"}, {"any", "value"}}, 3, false, "",
         "Записывает значение в элемент массива. То же, что `items[index] = value;`.\n\n```foxlang\nset(scores, 0, 100);\n```"},
        [](Call& c) {
            auto& items = c.array(0);
            items[checkedIndex(c, 1, items.size())] = c.at(2);
            return nothing();
        });
    add({"push", "void", {{"array", "items"}, {"any", "value"}}, 2, false, "",
         "Добавляет значение в конец массива.\n\n```foxlang\narray names;\npush(names, \"Лис\");\n```"},
        [](Call& c) {
            auto& items = c.array(0);
            if (items.size() >= maxElements) throw std::runtime_error("Runtime Error: array is too large");
            items.push_back(c.at(1));
            return nothing();
        });
    add({"pop", "any", {{"array", "items"}}, 1, false, "",
         "Удаляет и возвращает последний элемент массива. Пустой массив — ошибка выполнения."},
        [](Call& c) {
            auto& items = c.array(0);
            if (items.empty()) throw std::runtime_error("Runtime Error: pop() from an empty array");
            Value last = items.back();
            items.pop_back();
            return last;
        });
    add({"insert", "void", {{"array", "items"}, {"int", "index"}, {"any", "value"}}, 3, false, "",
         "Вставляет значение перед элементом `index`. Индекс, равный размеру, добавляет в конец."},
        [](Call& c) {
            auto& items = c.array(0);
            size_t index = checkedIndex(c, 1, items.size() + 1);
            if (items.size() >= maxElements) throw std::runtime_error("Runtime Error: array is too large");
            items.insert(items.begin() + static_cast<std::ptrdiff_t>(index), c.at(2));
            return nothing();
        });
    add({"remove_at", "any", {{"array", "items"}, {"int", "index"}}, 2, false, "",
         "Удаляет элемент по индексу и возвращает его; следующие элементы сдвигаются."},
        [](Call& c) {
            auto& items = c.array(0);
            size_t index = checkedIndex(c, 1, items.size());
            Value removed = items[index];
            items.erase(items.begin() + static_cast<std::ptrdiff_t>(index));
            return removed;
        });
    add({"resize", "void", {{"array", "items"}, {"int", "size"}}, 2, false, "",
         "Меняет размер массива. Новые элементы равны `0`, лишние отбрасываются."},
        [](Call& c) {
            c.array(0).resize(c.amount(1, maxElements), {"int", Text::integer(0)});
            return nothing();
        });

    // Types and conversions
    add({"type_of", "string", {{"any", "value"}}, 1, false, "",
         "Имя типа значения: `int`, `float`, `string`, `bool` или `array`."},
        [](Call& c) { return text(c.at(0).type); });
    add({"to_int", "int", {{"any", "value"}}, 1, false, "",
         "Преобразует в `int`: дробное число отбрасывает дробную часть, строка должна содержать число, "
         "`true`/`false` дают 1/0. Некорректная строка — ошибка выполнения, проверяйте её `is_number`.\n\n"
         "```foxlang\nint port = to_int(env_default(\"PORT\", \"8080\"));\n```"},
        [](Call& c) {
            const Value& value = c.at(0);
            if (value.type == "bool") return integer(c.flag(0) ? 1 : 0);
            if (value.type == "array") throw std::runtime_error("Type Error: to_int() cannot convert an array");
            double number = 0;
            if (value.type == "string" ? !parseNumber(value.value.str(), number) : !tryNumber(value, number))
                throw std::runtime_error("Runtime Error: to_int() cannot convert '" + value.value.str() + "' to a number");
            return Value{"int", intText(Value{"float", Text::real(std::trunc(number))}, "to_int() result")};
        });
    add({"to_float", "float", {{"any", "value"}}, 1, false, "",
         "Преобразует число, строку с числом или `bool` в `float`. Некорректная строка — ошибка выполнения."},
        [](Call& c) {
            const Value& value = c.at(0);
            if (value.type == "bool") return real(c.flag(0) ? 1.0 : 0.0);
            if (value.type == "array") throw std::runtime_error("Type Error: to_float() cannot convert an array");
            double number = 0;
            if (value.type == "string" ? !parseNumber(value.value.str(), number) : !tryNumber(value, number))
                throw std::runtime_error("Runtime Error: to_float() cannot convert '" + value.value.str() + "' to a number");
            return real(number);
        });
    add({"to_string", "string", {{"any", "value"}}, 1, false, "",
         "Текстовое представление значения; массив выводится как `[1, 2, 3]`."},
        [](Call& c) { return text(display(c.at(0), c.ctx)); });
    add({"is_number", "bool", {{"string", "text"}}, 1, false, "",
         "Проверяет, что строка целиком — число (`42`, `-3.5`, пробелы по краям допустимы).\n\n"
         "```foxlang\nif (is_number(answer)) { int n = to_int(answer); }\n```"},
        [](Call& c) {
            double ignored = 0;
            return boolean(parseNumber(c.text(0), ignored));
        });

    // Math
    add({"round", "int", {{"number", "value"}}, 1, false, "",
         "Округляет до ближайшего целого (половина — от нуля).\n\n```foxlang\nint r = round(3.6); // 4\n```"},
        [](Call& c) { return integer(static_cast<long long>(std::round(c.number(0)))); });
    add({"random", "int", {{"int", "min"}, {"int", "max"}}, 2, false, "",
         "Случайное целое от `min` до `max` включительно.\n\n```foxlang\nint dice = random(1, 6);\n```"},
        [](Call& c) {
            long long low = c.integer(0), high = c.integer(1);
            if (low > high)
                throw std::runtime_error("Runtime Error: random() lower bound " + std::to_string(low) +
                                         " is greater than upper bound " + std::to_string(high));
            std::uniform_int_distribution<long long> distribution(low, high);
            return integer(distribution(generator()));
        });
    add({"random_float", "float", {}, 0, false, "",
         "Случайное дробное число из полуинтервала [0, 1)."},
        [](Call&) { return real(std::uniform_real_distribution<double>(0.0, 1.0)(generator())); });
    add({"abs", "number", {{"number", "value"}}, 1, false, "",
         "Модуль числа. Сохраняет тип аргумента: `int` для `int`, иначе `float`."},
        [](Call& c) { return numberResult(c, std::fabs(c.number(0)), "abs"); });
    add({"min", "number", {{"number", "a"}, {"number", "b"}}, 2, false, "",
         "Меньшее из двух чисел; `int`, если оба аргумента `int`."},
        [](Call& c) { return numberResult(c, std::min(c.number(0), c.number(1)), "min"); });
    add({"max", "number", {{"number", "a"}, {"number", "b"}}, 2, false, "",
         "Большее из двух чисел; `int`, если оба аргумента `int`."},
        [](Call& c) { return numberResult(c, std::max(c.number(0), c.number(1)), "max"); });
    add({"clamp", "number", {{"number", "value"}, {"number", "min"}, {"number", "max"}}, 3, false, "",
         "Ограничивает число диапазоном; границы переставляются, если `min > max`. `int`, если все аргументы `int`."},
        [](Call& c) {
            double low = c.number(1), high = c.number(2);
            if (low > high) std::swap(low, high);
            return numberResult(c, std::max(low, std::min(c.number(0), high)), "clamp");
        });
    add({"sqrt", "float", {{"number", "value"}}, 1, false, "",
         "Квадратный корень. Отрицательный аргумент — ошибка выполнения."},
        [](Call& c) {
            double value = c.number(0);
            if (value < 0) throw std::runtime_error("Runtime Error: sqrt() of a negative number: " + c.text(0));
            return real(std::sqrt(value));
        });
    add({"pow", "float", {{"number", "base"}, {"number", "exponent"}}, 2, false, "",
         "Возведение в степень. Результат вне диапазона `float` — ошибка выполнения."},
        [](Call& c) {
            double result = std::pow(c.number(0), c.number(1));
            if (!std::isfinite(result)) throw std::runtime_error("Runtime Error: pow() result is out of the float range");
            return real(result);
        });
    add({"sin", "float", {{"number", "radians"}}, 1, false, "", "Синус угла в радианах."},
        [](Call& c) { return unaryMath(c, [](double v) { return std::sin(v); }); });
    add({"cos", "float", {{"number", "radians"}}, 1, false, "", "Косинус угла в радианах."},
        [](Call& c) { return unaryMath(c, [](double v) { return std::cos(v); }); });
    add({"tan", "float", {{"number", "radians"}}, 1, false, "", "Тангенс угла в радианах."},
        [](Call& c) { return unaryMath(c, [](double v) { return std::tan(v); }); });
    add({"asin", "float", {{"number", "value"}}, 1, false, "", "Арксинус в радианах; аргумент от -1 до 1."},
        [](Call& c) {
            double value = c.number(0);
            if (value < -1 || value > 1) throw std::runtime_error("Runtime Error: asin() argument must be -1..1");
            return real(std::asin(value));
        });
    add({"acos", "float", {{"number", "value"}}, 1, false, "", "Арккосинус в радианах; аргумент от -1 до 1."},
        [](Call& c) {
            double value = c.number(0);
            if (value < -1 || value > 1) throw std::runtime_error("Runtime Error: acos() argument must be -1..1");
            return real(std::acos(value));
        });
    add({"atan", "float", {{"number", "value"}}, 1, false, "", "Арктангенс в радианах."},
        [](Call& c) { return unaryMath(c, [](double v) { return std::atan(v); }); });
    add({"atan2", "float", {{"number", "y"}, {"number", "x"}}, 2, false, "",
         "Угол вектора (x, y) в радианах от -π до π; учитывает четверть плоскости."},
        [](Call& c) { return real(std::atan2(c.number(0), c.number(1))); });
    add({"floor", "float", {{"number", "value"}}, 1, false, "", "Округление вниз: `floor(-2.5)` даёт `-3`."},
        [](Call& c) { return unaryMath(c, [](double v) { return std::floor(v); }); });
    add({"ceil", "float", {{"number", "value"}}, 1, false, "", "Округление вверх: `ceil(2.1)` даёт `3`."},
        [](Call& c) { return unaryMath(c, [](double v) { return std::ceil(v); }); });
    add({"exp", "float", {{"number", "value"}}, 1, false, "", "Экспонента: e в степени `value`."},
        [](Call& c) { return unaryMath(c, [](double v) { return std::exp(v); }); });
    add({"log", "float", {{"number", "value"}}, 1, false, "", "Натуральный логарифм; аргумент больше нуля."},
        [](Call& c) {
            if (c.number(0) <= 0) throw std::runtime_error("Runtime Error: log() argument must be positive");
            return unaryMath(c, [](double v) { return std::log(v); });
        });
    add({"log10", "float", {{"number", "value"}}, 1, false, "", "Десятичный логарифм; аргумент больше нуля."},
        [](Call& c) {
            if (c.number(0) <= 0) throw std::runtime_error("Runtime Error: log10() argument must be positive");
            return unaryMath(c, [](double v) { return std::log10(v); });
        });
}

} // namespace foxlang::runtime
