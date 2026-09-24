// String and JSON builtins. Positions and lengths count characters (code points),
// never UTF-8 bytes, so Cyrillic and emoji behave like Latin text.
#include "Builtin.h"
#include <algorithm>
#include <stdexcept>

namespace foxlang::runtime {
namespace {

size_t sequenceLength(unsigned char lead) {
    if (lead < 0x80) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;
}

std::vector<std::string> characters(const std::string& text) {
    std::vector<std::string> out;
    for (size_t i = 0; i < text.size();) {
        size_t length = std::min(sequenceLength(static_cast<unsigned char>(text[i])), text.size() - i);
        out.push_back(text.substr(i, length));
        i += length;
    }
    return out;
}

size_t characterCount(const std::string& text) {
    size_t count = 0;
    for (unsigned char byte : text)
        if ((byte & 0xC0) != 0x80) ++count;
    return count;
}

// Byte offset of the character with the given index, or text.size() past the end.
size_t byteOffset(const std::string& text, size_t character) {
    size_t seen = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if ((static_cast<unsigned char>(text[i]) & 0xC0) == 0x80) continue;
        if (seen == character) return i;
        ++seen;
    }
    return text.size();
}

unsigned decode(const std::string& c) {
    auto byte = [&](size_t i) { return static_cast<unsigned>(static_cast<unsigned char>(c[i])); };
    switch (c.size()) {
        case 2: return ((byte(0) & 0x1F) << 6) | (byte(1) & 0x3F);
        case 3: return ((byte(0) & 0x0F) << 12) | ((byte(1) & 0x3F) << 6) | (byte(2) & 0x3F);
        case 4: return ((byte(0) & 0x07) << 18) | ((byte(1) & 0x3F) << 12) | ((byte(2) & 0x3F) << 6) | (byte(3) & 0x3F);
        default: return byte(0);
    }
}

std::string encode(unsigned cp) {
    std::string out;
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return out;
}

// Latin and Cyrillic letters, the alphabets FoxLang programs are written in.
std::string changeCase(const std::string& text, bool upper) {
    std::string out;
    out.reserve(text.size());
    for (const auto& c : characters(text)) {
        unsigned cp = decode(c);
        unsigned mapped = cp;
        if (upper) {
            if (cp >= 'a' && cp <= 'z') mapped = cp - 32;
            else if (cp >= 0x430 && cp <= 0x44F) mapped = cp - 0x20;
            else if (cp >= 0x450 && cp <= 0x45F) mapped = cp - 0x50;
        } else {
            if (cp >= 'A' && cp <= 'Z') mapped = cp + 32;
            else if (cp >= 0x410 && cp <= 0x42F) mapped = cp + 0x20;
            else if (cp >= 0x400 && cp <= 0x40F) mapped = cp + 0x50;
        }
        out += mapped == cp ? c : encode(mapped);
    }
    return out;
}

std::string scalarText(const Value& value, const std::string& what) {
    if (value.type == "array") throw std::runtime_error("Type Error: " + what + " cannot contain nested arrays");
    return value.value.str();
}

constexpr size_t maxText = size_t{64} * 1024 * 1024;

} // namespace

void addTextBuiltins(std::vector<Builtin>& out) {
    auto add = [&](BuiltinSpec spec, Handler handler) { out.push_back({std::move(spec), handler}); };

    add({"str_length", "int", {{"string", "text"}}, 1, false, "string",
         "Число символов строки. `size(text)` для строки считает байты UTF-8.\n\n"
         "```foxlang\nint n = str_length(\"Лисий\"); // 5, а size() даст 10\n```"},
        [](Call& c) { return integer(static_cast<long long>(characterCount(c.text(0)))); });
    add({"str_contains", "bool", {{"string", "text"}, {"string", "needle"}}, 2, false, "string",
         "Содержит ли `text` подстроку `needle`."},
        [](Call& c) { return boolean(c.text(0).find(c.text(1)) != std::string::npos); });
    add({"str_replace", "string", {{"string", "text"}, {"string", "from"}, {"string", "to"}}, 3, false, "string",
         "Заменяет все вхождения `from` на `to`. Пустой `from` оставляет строку без изменений."},
        [](Call& c) {
            std::string result = c.text(0);
            const std::string& from = c.text(1);
            const std::string& to = c.text(2);
            if (from.empty()) return text(result);
            for (size_t pos = 0; (pos = result.find(from, pos)) != std::string::npos; pos += to.size()) {
                result.replace(pos, from.size(), to);
                if (result.size() > maxText) throw std::runtime_error("Runtime Error: str_replace() result is too large");
            }
            return text(result);
        });
    add({"str_split", "array", {{"string", "text"}, {"string", "delimiter"}}, 2, false, "string",
         "Разбивает строку по разделителю и возвращает массив строк. Пустой разделитель делит на символы.\n\n"
         "```foxlang\narray parts = str_split(\"a,b,c\", \",\");\n```"},
        [](Call& c) {
            std::vector<Value> parts;
            const std::string& source = c.text(0);
            const std::string& delimiter = c.text(1);
            if (delimiter.empty()) {
                for (auto& character : characters(source)) parts.push_back(text(character));
            } else {
                size_t start = 0;
                for (size_t pos; (pos = source.find(delimiter, start)) != std::string::npos; start = pos + delimiter.size())
                    parts.push_back(text(source.substr(start, pos - start)));
                parts.push_back(text(source.substr(start)));
            }
            return Value{"array", c.ctx.newArray(std::move(parts))};
        });
    add({"str_join", "string", {{"array", "items"}, {"string", "separator"}}, 2, false, "string",
         "Склеивает элементы массива в строку через разделитель.\n\n"
         "```foxlang\nstring csv = str_join(parts, \";\");\n```"},
        [](Call& c) {
            std::string result;
            const auto& items = c.array(0);
            for (size_t i = 0; i < items.size(); ++i) {
                if (i > 0) result += c.text(1);
                result += scalarText(items[i], "str_join() items");
                if (result.size() > maxText) throw std::runtime_error("Runtime Error: str_join() result is too large");
            }
            return text(result);
        });
    add({"str_upper", "string", {{"string", "text"}}, 1, false, "string",
         "Строка прописными буквами (латиница и кириллица)."},
        [](Call& c) { return text(changeCase(c.text(0), true)); });
    add({"str_lower", "string", {{"string", "text"}}, 1, false, "string",
         "Строка строчными буквами (латиница и кириллица)."},
        [](Call& c) { return text(changeCase(c.text(0), false)); });
    add({"str_trim", "string", {{"string", "text"}}, 1, false, "string",
         "Убирает пробелы, табуляции и переводы строк по краям."},
        [](Call& c) {
            const std::string& source = c.text(0);
            auto first = source.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) return text("");
            return text(source.substr(first, source.find_last_not_of(" \t\r\n") - first + 1));
        });
    add({"str_starts_with", "bool", {{"string", "text"}, {"string", "prefix"}}, 2, false, "string",
         "Начинается ли строка с `prefix`."},
        [](Call& c) { return boolean(c.text(0).compare(0, c.text(1).size(), c.text(1)) == 0); });
    add({"str_ends_with", "bool", {{"string", "text"}, {"string", "suffix"}}, 2, false, "string",
         "Заканчивается ли строка на `suffix`."},
        [](Call& c) {
            const std::string& source = c.text(0);
            const std::string& suffix = c.text(1);
            return boolean(source.size() >= suffix.size() &&
                           source.compare(source.size() - suffix.size(), suffix.size(), suffix) == 0);
        });
    add({"str_index_of", "int", {{"string", "text"}, {"string", "needle"}}, 2, false, "string",
         "Номер символа, с которого начинается первое вхождение `needle`, или `-1`."},
        [](Call& c) {
            size_t found = c.text(0).find(c.text(1));
            if (found == std::string::npos) return integer(-1);
            return integer(static_cast<long long>(characterCount(c.text(0).substr(0, found))));
        });
    add({"str_substring", "string", {{"string", "text"}, {"int", "start"}, {"int", "length"}}, 2, false, "string",
         "Подстрока из `length` символов начиная с символа `start`; без `length` — до конца строки. "
         "Выход за границы строки укорачивает результат.\n\n```foxlang\nstring word = str_substring(\"Привет, мир\", 8, 3); // \"мир\"\n```"},
        [](Call& c) {
            const std::string& source = c.text(0);
            size_t start = c.amount(1, maxText);
            size_t from = byteOffset(source, start);
            if (!c.has(2)) return text(source.substr(from));
            size_t to = byteOffset(source, start + c.amount(2, maxText));
            return text(source.substr(from, to - from));
        });
    add({"str_repeat", "string", {{"string", "text"}, {"int", "count"}}, 2, false, "string",
         "Повторяет строку `count` раз."},
        [](Call& c) {
            const std::string& unit = c.text(0);
            size_t count = c.amount(1, maxText);
            if (!unit.empty() && count > maxText / unit.size())
                throw std::runtime_error("Runtime Error: str_repeat() result is too large");
            std::string result;
            result.reserve(unit.size() * count);
            for (size_t i = 0; i < count; ++i) result += unit;
            return text(result);
        });

    add({"json_get", "string", {{"string", "json"}, {"string", "path"}}, 2, false, "json",
         "Значение по точечному пути: ключи объектов и индексы массивов (`message.chat.id`, `items.0.name`). "
         "Строки декодируются (включая `\\uXXXX` и emoji), объекты и массивы возвращаются как JSON-текст. "
         "Отсутствующий путь даёт пустую строку."},
        [](Call& c) { return jsonGet(c.text(0), c.text(1)); });
    add({"json_escape", "string", {{"string", "text"}}, 1, false, "json",
         "Экранирует кавычки, обратную косую черту и управляющие символы для вставки внутрь JSON-строки."},
        [](Call& c) { return jsonEscape(c.text(0)); });
    add({"json_count", "int", {{"string", "json"}, {"string", "path"}}, 2, false, "json",
         "Число элементов массива или ключей объекта по пути; `-1`, если там нет массива или объекта. "
         "Пустой путь означает весь документ."},
        [](Call& c) { return integer(jsonCount(c.text(0), c.text(1))); });
    add({"json_type", "string", {{"string", "json"}, {"string", "path"}}, 2, false, "json",
         "Тип значения по пути: `object`, `array`, `string`, `number`, `bool`, `null` или пустая строка, если пути нет."},
        [](Call& c) { return text(jsonType(c.text(0), c.text(1))); });
}

} // namespace foxlang::runtime
