#pragma once

#include "core/adapter.hpp"
#include "core/dom.hpp"
#include "json.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace shop::sites {

/// Trims leading and trailing whitespace and collapses repeated whitespace into a single space.
inline std::string cleanText(std::string_view value) {
    std::string s;
    s.reserve(value.size());
    bool inWs = false;
    for (char c : value) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!inWs && !s.empty()) {
                s.push_back(' ');
                inWs = true;
            }
        } else {
            s.push_back(c);
            inWs = false;
        }
    }
    while (!s.empty() && s.back() == ' ') {
        s.pop_back();
    }
    return s;
}

inline std::string toLower(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

inline double round2(double val) {
    return std::round(val * 100.0) / 100.0;
}

inline std::string formatEuro2(double val) {
    return std::format("€{:.2f}", val);
}

inline std::string urlEncode(std::string_view str, bool plusForSpace = false) {
    std::string out;
    out.reserve(str.size() * 3);
    for (char c : str) {
        unsigned char uc = static_cast<unsigned char>(c);
        if ((uc >= 'a' && uc <= 'z') || (uc >= 'A' && uc <= 'Z') ||
            (uc >= '0' && uc <= '9') || uc == '-' || uc == '_' || uc == '.' || uc == '~') {
            out.push_back(c);
        } else if (plusForSpace && c == ' ') {
            out.push_back('+');
        } else {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%%%02X", uc);
            out.append(buf);
        }
    }
    return out;
}

inline std::string urlDecode(std::string_view str, bool plusToSpace = false) {
    std::string out;
    out.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            auto fromHex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            int h1 = fromHex(str[i + 1]);
            int h2 = fromHex(str[i + 2]);
            if (h1 >= 0 && h2 >= 0) {
                out.push_back(static_cast<char>((h1 << 4) | h2));
                i += 2;
                continue;
            }
        } else if (plusToSpace && str[i] == '+') {
            out.push_back(' ');
            continue;
        }
        out.push_back(str[i]);
    }
    return out;
}

inline std::optional<std::string> getUrlParam(std::string_view url, std::string_view param) {
    size_t qpos = url.find('?');
    if (qpos == std::string_view::npos) return std::nullopt;
    std::string_view query = url.substr(qpos + 1);
    size_t hashPos = query.find('#');
    if (hashPos != std::string_view::npos) query = query.substr(0, hashPos);
    while (!query.empty()) {
        size_t amp = query.find('&');
        std::string_view pair = (amp == std::string_view::npos) ? query : query.substr(0, amp);
        size_t eq = pair.find('=');
        std::string_view key = (eq == std::string_view::npos) ? pair : pair.substr(0, eq);
        std::string_view val = (eq == std::string_view::npos) ? "" : pair.substr(eq + 1);
        if (key == param) {
            return urlDecode(val, false);
        }
        if (amp == std::string_view::npos) break;
        query = query.substr(amp + 1);
    }
    return std::nullopt;
}

inline std::string getUrlHost(std::string_view url) {
    size_t schemePos = url.find("://");
    std::string_view rest = (schemePos != std::string_view::npos) ? url.substr(schemePos + 3) : url;
    size_t slashPos = rest.find('/');
    if (slashPos != std::string_view::npos) rest = rest.substr(0, slashPos);
    size_t qPos = rest.find('?');
    if (qPos != std::string_view::npos) rest = rest.substr(0, qPos);
    size_t colonPos = rest.find(':');
    if (colonPos != std::string_view::npos) rest = rest.substr(0, colonPos);
    return std::string(rest);
}

inline void appendUtf8(std::string& out, uint32_t cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

inline std::string decodeHtmlEntities(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '&') {
            size_t semi = s.find(';', i);
            if (semi != std::string_view::npos && semi - i <= 10) {
                std::string_view entity = s.substr(i + 1, semi - i - 1);
                if (entity == "amp") { out.push_back('&'); i = semi + 1; continue; }
                if (entity == "quot") { out.push_back('"'); i = semi + 1; continue; }
                if (entity == "apos") { out.push_back('\''); i = semi + 1; continue; }
                if (entity == "lt") { out.push_back('<'); i = semi + 1; continue; }
                if (entity == "gt") { out.push_back('>'); i = semi + 1; continue; }
                if (!entity.empty() && entity[0] == '#') {
                    uint32_t cp = 0;
                    if (entity.size() > 1 && (entity[1] == 'x' || entity[1] == 'X')) {
                        try {
                            cp = std::stoul(std::string(entity.substr(2)), nullptr, 16);
                        } catch (...) {}
                    } else {
                        try {
                            cp = std::stoul(std::string(entity.substr(1)), nullptr, 10);
                        } catch (...) {}
                    }
                    if (cp > 0) {
                        appendUtf8(out, cp);
                        i = semi + 1;
                        continue;
                    }
                }
            }
        }
        out.push_back(s[i++]);
    }
    return out;
}

inline std::string base64UrlDecode(std::string_view input) {
    std::string b64;
    b64.reserve(input.size() + 4);
    for (char c : input) {
        if (c == '-') b64.push_back('+');
        else if (c == '_') b64.push_back('/');
        else if (c != '=' && c != '\r' && c != '\n' && c != ' ') b64.push_back(c);
    }
    while (b64.size() % 4 != 0) {
        b64.push_back('=');
    }
    static const int T[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1, 0,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    std::string out;
    out.reserve((b64.size() / 4) * 3);
    for (size_t i = 0; i < b64.size(); i += 4) {
        int v0 = T[static_cast<unsigned char>(b64[i])];
        int v1 = T[static_cast<unsigned char>(b64[i + 1])];
        int v2 = b64[i + 2] == '=' ? 0 : T[static_cast<unsigned char>(b64[i + 2])];
        int v3 = b64[i + 3] == '=' ? 0 : T[static_cast<unsigned char>(b64[i + 3])];
        if (v0 < 0 || v1 < 0) break;
        uint32_t triple = (static_cast<uint32_t>(v0) << 18) |
                          (static_cast<uint32_t>(v1) << 12) |
                          (static_cast<uint32_t>(v2) << 6) |
                          static_cast<uint32_t>(v3);
        out.push_back(static_cast<char>((triple >> 16) & 0xFF));
        if (b64[i + 2] != '=') {
            out.push_back(static_cast<char>((triple >> 8) & 0xFF));
        }
        if (b64[i + 3] != '=') {
            out.push_back(static_cast<char>(triple & 0xFF));
        }
    }
    return out;
}

} // namespace shop::sites
