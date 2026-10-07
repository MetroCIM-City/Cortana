#include "rfi/Store.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cwchar>
#include <string>
#include <string_view>

namespace rfi {
namespace {

constexpr const char* kKnown[] = {
    "discipline", "location", "originator", "subDiscipline",
    "documentType", "program", "subProgram", "modifiedUtc", "source", "schema"};

bool IsKnown(std::string_view key) {
    for (const char* name : kKnown) {
        if (key == name) return true;
    }
    return false;
}

std::string WideToUtf8(const std::wstring& value, bool* bad) {
    *bad = false;
    if (value.empty()) return {};
    const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                          static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) {
        *bad = true;
        return {};
    }
    std::string out(static_cast<size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                        out.data(), bytes, nullptr, nullptr);
    return out;
}

std::wstring Utf8ToWide(std::string_view value, bool* bad) {
    *bad = false;
    if (value.empty()) return {};
    const int chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                          static_cast<int>(value.size()), nullptr, 0);
    if (chars <= 0) {
        *bad = true;
        return {};
    }
    std::wstring out(static_cast<size_t>(chars), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                        out.data(), chars);
    return out;
}

void AppendEscaped(std::string& out, const std::wstring& value, bool* bad) {
    const std::string utf8 = WideToUtf8(value, bad);
    out.push_back('"');
    for (unsigned char c : utf8) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(static_cast<char>(c));
    }
    out.push_back('"');
}

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    Status Parse(FileInfo& info) {
        if (text_.size() >= 3 && static_cast<unsigned char>(text_[0]) == 0xEF &&
            static_cast<unsigned char>(text_[1]) == 0xBB && static_cast<unsigned char>(text_[2]) == 0xBF) {
            index_ = 3;
        }
        SkipWs();
        if (!Consume('{')) return Fail(L"payload is not a JSON object");
        bool first = true;
        SkipWs();
        if (Consume('}')) return Status{S_OK, {}};
        while (index_ < text_.size()) {
            if (!first && !Consume(',')) return Fail(L"expected comma");
            first = false;
            SkipWs();
            std::string key;
            if (!ParseString(key)) return Fail(L"expected property name");
            SkipWs();
            if (!Consume(':')) return Fail(L"expected colon");
            SkipWs();
            if (key == "schema") {
                if (!ParseSchema(info.schema)) return Fail(L"schema must be a number");
            } else if (IsKnown(key) && key != "schema") {
                std::string raw;
                if (!ParseString(raw)) return Fail(L"expected string value");
                bool bad = false;
                std::wstring wide = Utf8ToWide(raw, &bad);
                if (bad) return Fail(L"value is not UTF-8");
                bool tooLong = false;
                wide = SanitizeField(wide, &tooLong);
                if (tooLong) return Fail(L"stored value exceeds 256 characters");
                Assign(info, key, wide);
            } else {
                std::string raw;
                if (!CaptureRaw(raw)) return Fail(L"invalid JSON value");
                info.unknown.emplace_back(key, raw);
            }
            SkipWs();
            if (Consume('}')) {
                SkipWs();
                if (index_ != text_.size()) return Fail(L"trailing data");
                return Status{S_OK, {}};
            }
        }
        return Fail(L"unterminated object");
    }

private:
    Status Fail(const wchar_t* message) const { return Status{E_INVALIDARG, message}; }

    void SkipWs() {
        while (index_ < text_.size()) {
            const char c = text_[index_];
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
            ++index_;
        }
    }

    bool Consume(char expected) {
        if (index_ < text_.size() && text_[index_] == expected) {
            ++index_;
            return true;
        }
        return false;
    }

    bool ParseString(std::string& out) {
        if (!Consume('"')) return false;
        out.clear();
        while (index_ < text_.size()) {
            const unsigned char c = static_cast<unsigned char>(text_[index_++]);
            if (c == '"') return true;
            if (c == '\\') {
                if (index_ >= text_.size()) return false;
                const char esc = text_[index_++];
                if (esc == '"' || esc == '\\' || esc == '/') out.push_back(esc);
                else if (esc == 'b') out.push_back('\b');
                else if (esc == 'f') out.push_back('\f');
                else if (esc == 'n') out.push_back('\n');
                else if (esc == 'r') out.push_back('\r');
                else if (esc == 't') out.push_back('\t');
                else if (esc == 'u') {
                    if (!ParseUnicode(out)) return false;
                } else {
                    return false;
                }
            } else if (c < 0x20) {
                return false;
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
        return false;
    }

    bool ParseUnicode(std::string& out) {
        unsigned code = 0;
        if (!ReadHex(code)) return false;
        if (code >= 0xD800 && code <= 0xDBFF) {
            if (index_ + 6 <= text_.size() && text_[index_] == '\\' && text_[index_ + 1] == 'u') {
                index_ += 2;
                unsigned low = 0;
                if (!ReadHex(low) || low < 0xDC00 || low > 0xDFFF) return false;
                code = 0x10000 + (((code - 0xD800) << 10) | (low - 0xDC00));
            } else {
                return false;
            }
        }
        AppendUtf8(out, code);
        return true;
    }

    bool ReadHex(unsigned& code) {
        if (index_ + 4 > text_.size()) return false;
        code = 0;
        for (int n = 0; n < 4; ++n) {
            const char c = text_[index_++];
            code <<= 4;
            if (c >= '0' && c <= '9') code += static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') code += static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') code += static_cast<unsigned>(c - 'A' + 10);
            else return false;
        }
        return true;
    }

    static void AppendUtf8(std::string& out, unsigned code) {
        if (code <= 0x7F) {
            out.push_back(static_cast<char>(code));
        } else if (code <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (code >> 6)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else if (code <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | (code >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (code >> 18)));
            out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
    }

    bool CaptureRaw(std::string& raw) {
        const size_t start = index_;
        if (index_ >= text_.size()) return false;
        const char c = text_[index_];
        if (c == '"') {
            std::string ignore;
            if (!ParseString(ignore)) return false;
        } else if (c == '{' || c == '[') {
            if (!SkipContainer(c, c == '{' ? '}' : ']')) return false;
        } else {
            while (index_ < text_.size()) {
                const char n = text_[index_];
                if (n == ',' || n == '}' || n == ']' || n == ' ' || n == '\t' || n == '\r' || n == '\n') break;
                ++index_;
            }
            if (index_ == start) return false;
        }
        raw.assign(text_.substr(start, index_ - start));
        return !raw.empty();
    }

    bool SkipContainer(char open, char close) {
        int depth = 0;
        bool inString = false;
        bool escape = false;
        for (; index_ < text_.size(); ++index_) {
            const char c = text_[index_];
            if (inString) {
                if (escape) escape = false;
                else if (c == '\\') escape = true;
                else if (c == '"') inString = false;
                continue;
            }
            if (c == '"') inString = true;
            else if (c == open) ++depth;
            else if (c == close) {
                --depth;
                if (depth == 0) {
                    ++index_;
                    return true;
                }
            }
        }
        return false;
    }

    bool ParseSchema(int& schema) {
        const size_t start = index_;
        if (index_ < text_.size() && text_[index_] == '-') ++index_;
        if (index_ >= text_.size() || text_[index_] < '0' || text_[index_] > '9') return false;
        while (index_ < text_.size() && text_[index_] >= '0' && text_[index_] <= '9') ++index_;
        try {
            schema = std::stoi(std::string(text_.substr(start, index_ - start)));
        } catch (...) {
            return false;
        }
        return true;
    }

    static void Assign(FileInfo& info, std::string_view key, const std::wstring& value) {
        if (key == "discipline") {
            info.discipline = value;
            info.mask |= FieldDiscipline;
        } else if (key == "location") {
            info.location = value;
            info.mask |= FieldLocation;
        } else if (key == "originator") {
            info.originator = value;
            info.mask |= FieldOriginator;
        } else if (key == "subDiscipline") {
            info.subDiscipline = value;
            info.mask |= FieldSubDiscipline;
        } else if (key == "documentType") {
            info.documentType = value;
            info.mask |= FieldDocumentType;
        } else if (key == "program") {
            info.program = value;
            info.mask |= FieldProgram;
        } else if (key == "subProgram") {
            info.subProgram = value;
            info.mask |= FieldSubProgram;
        } else if (key == "modifiedUtc") {
            info.modifiedUtc = value;
        } else if (key == "source") {
            info.source = value;
        }
    }

    std::string_view text_;
    size_t index_ = 0;
};

}  // namespace

std::wstring SanitizeField(const std::wstring& value, bool* tooLong) {
    std::wstring out;
    out.reserve(value.size());
    for (wchar_t c : value) {
        if (c < 0x20 || c == 0x7F) continue;
        out.push_back(c);
    }
    size_t begin = 0;
    while (begin < out.size() && out[begin] == L' ') ++begin;
    size_t end = out.size();
    while (end > begin && out[end - 1] == L' ') --end;
    out = out.substr(begin, end - begin);
    if (tooLong) *tooLong = out.size() > kMaxValueChars;
    return out;
}

std::wstring NowUtc() {
    SYSTEMTIME time{};
    GetSystemTime(&time);
    wchar_t buffer[32];
    swprintf_s(buffer, L"%04u-%02u-%02uT%02u:%02u:%02uZ", time.wYear, time.wMonth, time.wDay, time.wHour,
               time.wMinute, time.wSecond);
    return buffer;
}

Status Encode(const FileInfo& info, std::string& utf8) {
    bool bad = false;
    std::string out;
    out.reserve(512);
    out += "{\"schema\":1";
    auto field = [&](const char* name, const std::wstring& value) {
        out += ",\"";
        out += name;
        out += "\":";
        AppendEscaped(out, value, &bad);
    };
    field("discipline", info.discipline);
    field("location", info.location);
    field("originator", info.originator);
    field("subDiscipline", info.subDiscipline);
    field("documentType", info.documentType);
    field("program", info.program);
    field("subProgram", info.subProgram);
    field("modifiedUtc", info.modifiedUtc);
    field("source", info.source);
    for (const auto& extra : info.unknown) {
        if (IsKnown(extra.first)) continue;
        out += ",\"";
        out += extra.first;
        out += "\":";
        out += extra.second;
    }
    out += "}";
    if (bad) return Status{E_INVALIDARG, L"value is not valid Unicode"};
    if (out.size() > kMaxPayloadBytes) return Status{E_INVALIDARG, L"payload exceeds 4 KB"};
    utf8 = std::move(out);
    return Status{S_OK, {}};
}

Status Decode(const std::string& utf8, FileInfo& info) {
    if (utf8.size() > kMaxPayloadBytes) return Status{E_INVALIDARG, L"payload exceeds 4 KB"};
    info = FileInfo{};
    return Parser(utf8).Parse(info);
}

}  // namespace rfi
