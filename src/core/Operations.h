#pragma once
// What the language's operators, indexes and fields do to values, apart from the VM's
// fast paths: the slow cases, the conversions and every error message.
#include "foxlang/Context.h"
#include "foxlang/Runtime.h"
#include <string>

namespace foxlang::runtime {

// A binary operator other than && and ||. `text` is the operator as written, for messages.
Value binary(Operator op, const std::string& text, const Value& left, const Value& right);
// The same on two ints, the common case, without leaving the caller's code.
inline bool binaryInts(Operator op, long long l, long long r, const std::string& text, Value& out);

// `side` is "left" or "right": "Type Error: left operand of '&&' must be bool, got 'int'".
bool operandTruth(const Value& value, const char* side, const std::string& text);
Value negate(const Value& value);
Value logicalNot(const Value& value);

// A map key: the text of a string or an int.
std::string keyOf(const Value& key);
// base[index] on a container value; create inserts a new map key.
Value& element(Value& base, const Value& index, bool create);
// base.name on a struct or a map; for a struct field, *declared is its declared type.
Value& member(Value& base, const std::string& name, bool create, const std::string** declared = nullptr);
// name++ or name-- on a variable: returns the old value.
Value postIncrement(Value& target, int delta, const std::string& name);
// Whether a switch value matches a case: numbers by value, the rest by content.
bool switchMatches(const Value& value, const Value& candidate);

[[noreturn]] void divisionByZero();

inline bool binaryInts(Operator op, long long l, long long r, const std::string& text, Value& out) {
    switch (op) {
        case Operator::Add: out = intResult(l + r, text); return true;
        case Operator::Sub: out = intResult(l - r, text); return true;
        case Operator::Mul: out = intResult(l * r, text); return true;
        case Operator::Div:
            if (r == 0) divisionByZero();
            out = intResult(l / r, text);
            return true;
        case Operator::Mod:
            if (r == 0) divisionByZero();
            out = intResult(l % r, text);
            return true;
        case Operator::Eq: out = Value::boolean(l == r); return true;
        case Operator::Ne: out = Value::boolean(l != r); return true;
        case Operator::Lt: out = Value::boolean(l < r); return true;
        case Operator::Le: out = Value::boolean(l <= r); return true;
        case Operator::Gt: out = Value::boolean(l > r); return true;
        case Operator::Ge: out = Value::boolean(l >= r); return true;
        default: return false;
    }
}

} // namespace foxlang::runtime
