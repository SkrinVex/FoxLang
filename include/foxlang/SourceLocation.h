#pragma once
#include <stdexcept>
#include <string>
#include <vector>
#include <sstream>
#include <cstdint>

namespace foxlang {

struct SourcePosition {
    int line = 1;       // 1-based line number
    int column = 1;     // 1-based column (measured in UTF-16 code units + 1)
    size_t byteOffset = 0; // 0-based byte offset in UTF-8 source string

    bool operator==(const SourcePosition& other) const {
        return line == other.line && column == other.column && byteOffset == other.byteOffset;
    }

    bool operator!=(const SourcePosition& other) const {
        return !(*this == other);
    }

    bool isBefore(const SourcePosition& other) const {
        if (line != other.line) return line < other.line;
        return column < other.column;
    }
};

struct SourceRange {
    SourcePosition start;
    SourcePosition end;

    bool contains(int l, int col) const {
        if (l < start.line || l > end.line) return false;
        if (l == start.line && col < start.column) return false;
        if (l == end.line && col > end.column) return false;
        return true;
    }

    bool contains(const SourcePosition& pos) const {
        return contains(pos.line, pos.column);
    }
};

// Utilities for converting between UTF-8 bytes and LSP UTF-16 positions
namespace utf {

// Computes the number of UTF-16 code units for a single UTF-8 sequence starting at 'leadByte'
inline int utf8SequenceUtf16Units(unsigned char lead) {
    if (lead < 0x80) return 1;           // 1-byte ASCII -> 1 UTF-16 code unit
    if ((lead & 0xE0) == 0xC0) return 1; // 2-byte UTF-8 (<= U+07FF) -> 1 UTF-16 code unit
    if ((lead & 0xF0) == 0xE0) return 1; // 3-byte UTF-8 (<= U+FFFF) -> 1 UTF-16 code unit
    if ((lead & 0xF8) == 0xF0) return 2; // 4-byte UTF-8 (> U+FFFF, e.g. emoji) -> surrogate pair (2 UTF-16 code units)
    return 1;
}

// Converts 1-based (line, column) to LSP 0-based (line, character)
// Since column is defined as (UTF-16 code units from start of line + 1),
// LSP character is simply (column - 1).
inline int columnToLspCharacter(int column) {
    return column > 0 ? column - 1 : 0;
}

inline int lspCharacterToColumn(int character) {
    return character + 1;
}

inline int lineToLspLine(int line) {
    return line > 0 ? line - 1 : 0;
}

inline int lspLineToLine(int lspLine) {
    return lspLine + 1;
}

// Finds the byte offset in a UTF-8 string for a given LSP 0-based line and character (in UTF-16 code units)
inline size_t lspPositionToByteOffset(const std::string& source, int lspLine, int lspChar) {
    int curLine = 0;
    size_t i = 0;
    const size_t len = source.size();

    // Advance to target line
    while (i < len && curLine < lspLine) {
        if (source[i] == '\n') {
            curLine++;
        }
        i++;
    }

    if (curLine != lspLine) return len;

    // Advance by UTF-16 code units within the line
    int curUtf16 = 0;
    while (i < len && source[i] != '\n' && curUtf16 < lspChar) {
        unsigned char b = static_cast<unsigned char>(source[i]);
        if (b < 0x80) {
            curUtf16 += 1;
            i += 1;
        } else if ((b & 0xE0) == 0xC0 && i + 1 < len) {
            curUtf16 += 1;
            i += 2;
        } else if ((b & 0xF0) == 0xE0 && i + 2 < len) {
            curUtf16 += 1;
            i += 3;
        } else if ((b & 0xF8) == 0xF0 && i + 3 < len) {
            curUtf16 += 2;
            i += 4;
        } else {
            curUtf16 += 1;
            i += 1;
        }
    }

    return i;
}

// Converts a byte offset in UTF-8 source to SourcePosition (1-based line & column)
inline SourcePosition byteOffsetToSourcePosition(const std::string& source, size_t offset) {
    SourcePosition pos;
    pos.line = 1;
    pos.column = 1;
    pos.byteOffset = offset > source.size() ? source.size() : offset;

    size_t i = 0;
    const size_t target = pos.byteOffset;
    while (i < target) {
        char ch = source[i];
        if (ch == '\n') {
            pos.line++;
            pos.column = 1;
            i++;
            continue;
        }

        unsigned char b = static_cast<unsigned char>(ch);
        if (b < 0x80) {
            pos.column += 1;
            i += 1;
        } else if ((b & 0xE0) == 0xC0 && i + 1 <= target) {
            pos.column += 1;
            i += 2;
        } else if ((b & 0xF0) == 0xE0 && i + 2 <= target) {
            pos.column += 1;
            i += 3;
        } else if ((b & 0xF8) == 0xF0 && i + 3 <= target) {
            pos.column += 2; // 4-byte UTF-8 emoji counts as 2 in UTF-16
            i += 4;
        } else {
            pos.column += 1;
            i += 1;
        }
    }

    return pos;
}

} // namespace utf

enum class DiagnosticSeverity {
    Error = 1,
    Warning = 2,
    Information = 3,
    Hint = 4
};

struct Diagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    std::string message;
    SourceRange range;
    std::string source = "foxlang";
    int code = 0;

    std::string severityString() const {
        switch (severity) {
            case DiagnosticSeverity::Error: return "error";
            case DiagnosticSeverity::Warning: return "warning";
            case DiagnosticSeverity::Information: return "info";
            case DiagnosticSeverity::Hint: return "hint";
        }
        return "error";
    }

    std::string format(const std::string& filename = "") const {
        std::ostringstream ss;
        if (!filename.empty()) ss << filename << ":";
        ss << range.start.line << ":" << range.start.column << ": "
           << severityString() << ": " << message;
        return ss.str();
    }
};

// A syntax error that knows where it is. what() reads "file:line: message" once a
// file is known and "message at line N" before that; message() is the bare text
// editors show next to the range they already have.
class SyntaxError : public std::runtime_error {
public:
    SyntaxError(std::string message, int line, std::string file = "")
        : std::runtime_error(message), line(line), text(std::move(message)) {
        setFile(std::move(file));
    }
    const char* what() const noexcept override { return full.c_str(); }
    const std::string& message() const { return text; }
    const std::string& file() const { return fileName; }
    void setFile(std::string name) {
        fileName = std::move(name);
        full = fileName.empty() ? text + " at line " + std::to_string(line)
                                : fileName + ":" + std::to_string(line) + ": " + text;
    }
    int line;

private:
    std::string text, fileName, full;
};

} // namespace foxlang
