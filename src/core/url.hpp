#pragma once

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace shop::core {

struct ParsedUrl {
    std::string scheme;
    std::string userinfo;
    std::string host;
    std::string port;
    std::string path;
    std::optional<std::string> query;
    std::optional<std::string> fragment;

    std::string toString() const {
        std::string out;
        out.reserve(scheme.size() + 3 + userinfo.size() + host.size() + port.size() + path.size() + 16);
        out += scheme;
        out += "://";
        if (!userinfo.empty()) {
            out += userinfo;
            out += '@';
        }
        out += host;
        if (!port.empty()) {
            out += ':';
            out += port;
        }
        out += path.empty() ? "/" : path;
        if (query.has_value()) {
            out += '?';
            out += *query;
        }
        if (fragment.has_value()) {
            out += '#';
            out += *fragment;
        }
        return out;
    }
};

namespace detail {

inline bool isSchemeChar(char c, bool first) {
    unsigned char uc = static_cast<unsigned char>(c);
    if (std::isalpha(uc) != 0)
        return true;
    if (first)
        return false;
    return std::isdigit(uc) != 0 || c == '+' || c == '-' || c == '.';
}

inline std::string toLowerAscii(std::string_view s) {
    std::string out(s);
    for (char &c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

inline bool parseAuthority(std::string_view auth, ParsedUrl &out) {
    if (auth.empty())
        return false;
    size_t at = auth.rfind('@');
    std::string_view hostport = auth;
    if (at != std::string_view::npos) {
        out.userinfo = std::string(auth.substr(0, at));
        hostport = auth.substr(at + 1);
    }
    if (hostport.empty())
        return false;
    if (hostport.front() == '[') {
        size_t close = hostport.find(']');
        if (close == std::string_view::npos || close == 1)
            return false;
        out.host = std::string(hostport.substr(0, close + 1));
        std::string_view rest = hostport.substr(close + 1);
        if (!rest.empty()) {
            if (rest.front() != ':')
                return false;
            rest.remove_prefix(1);
            if (rest.empty())
                return false;
            for (char c : rest) {
                if (std::isdigit(static_cast<unsigned char>(c)) == 0)
                    return false;
            }
            out.port = std::string(rest);
        }
        return true;
    }
    size_t colon = hostport.find(':');
    if (colon != std::string_view::npos) {
        if (colon == 0)
            return false;
        std::string_view port = hostport.substr(colon + 1);
        for (char c : port) {
            if (std::isdigit(static_cast<unsigned char>(c)) == 0)
                return false;
        }
        out.host = std::string(hostport.substr(0, colon));
        out.port = std::string(port);
    } else {
        out.host = std::string(hostport);
    }
    return !out.host.empty();
}

inline std::string removeDotSegments(std::string_view input) {
    std::vector<std::string_view> stack;
    bool trailingSlash = false;
    size_t pos = 0;
    while (pos <= input.size()) {
        size_t slash = input.find('/', pos);
        bool last = (slash == std::string_view::npos);
        std::string_view seg = last ? input.substr(pos) : input.substr(pos, slash - pos);
        if (seg == "..") {
            if (!stack.empty())
                stack.pop_back();
            trailingSlash = true;
        } else if (seg == ".") {
            trailingSlash = true;
        } else if (!seg.empty()) {
            stack.push_back(seg);
            trailingSlash = !last;
        } else {
            trailingSlash = true;
        }
        if (last)
            break;
        pos = slash + 1;
    }
    std::string out = "/";
    for (size_t i = 0; i < stack.size(); ++i) {
        if (i > 0)
            out += '/';
        out += stack[i];
    }
    if (trailingSlash && out.size() > 1)
        out += '/';
    return out;
}

inline void splitPathQueryFrag(
    std::string_view rest,
    std::string &path,
    std::optional<std::string> &query,
    std::optional<std::string> &fragment) {
    size_t hash = rest.find('#');
    if (hash != std::string_view::npos) {
        fragment = std::string(rest.substr(hash + 1));
        rest = rest.substr(0, hash);
    }
    size_t q = rest.find('?');
    if (q != std::string_view::npos) {
        query = std::string(rest.substr(q + 1));
        rest = rest.substr(0, q);
    }
    path = std::string(rest);
}

inline bool hasValidSchemePrefix(std::string_view s, size_t &colonPos) {
    size_t colon = s.find(':');
    if (colon == std::string_view::npos || colon == 0)
        return false;
    for (size_t i = 0; i < colon; ++i) {
        if (!isSchemeChar(s[i], i == 0))
            return false;
    }
    colonPos = colon;
    return true;
}

} // namespace detail

inline std::optional<ParsedUrl> parseUrl(std::string_view input) {
    if (input.empty())
        return std::nullopt;
    for (unsigned char c : input) {
        if (c <= 0x20 || c == 0x7F)
            return std::nullopt;
    }
    size_t colon = 0;
    if (!detail::hasValidSchemePrefix(input, colon))
        return std::nullopt;
    std::string_view afterColon = input.substr(colon + 1);
    if (!afterColon.starts_with("//"))
        return std::nullopt;
    std::string_view rest = afterColon.substr(2);

    size_t authEnd = rest.find_first_of("/?#");
    std::string_view auth = (authEnd == std::string_view::npos) ? rest : rest.substr(0, authEnd);
    std::string_view tail = (authEnd == std::string_view::npos) ? std::string_view{} : rest.substr(authEnd);

    ParsedUrl out;
    out.scheme = detail::toLowerAscii(input.substr(0, colon));
    if (!detail::parseAuthority(auth, out))
        return std::nullopt;
    detail::splitPathQueryFrag(tail, out.path, out.query, out.fragment);
    if (!out.path.empty()) {
        out.path = detail::removeDotSegments(out.path);
    } else {
        out.path = "/";
    }
    return out;
}

inline std::optional<ParsedUrl> resolveUrl(std::string_view baseStr, std::string_view ref) {
    auto base = parseUrl(baseStr);
    if (!base)
        return std::nullopt;
    if (ref.empty())
        return base;
    for (unsigned char c : ref) {
        if (c <= 0x20 || c == 0x7F)
            return std::nullopt;
    }

    size_t colon = 0;
    if (detail::hasValidSchemePrefix(ref, colon)) {
        return parseUrl(ref);
    }

    if (ref.starts_with("//")) {
        return parseUrl(base->scheme + ":" + std::string(ref));
    }

    ParsedUrl out;
    out.scheme = base->scheme;
    out.userinfo = base->userinfo;
    out.host = base->host;
    out.port = base->port;

    if (ref.front() == '#') {
        out.path = base->path;
        out.query = base->query;
        out.fragment = std::string(ref.substr(1));
        return out;
    }

    if (ref.front() == '?') {
        out.path = base->path;
        std::string dummy;
        detail::splitPathQueryFrag(ref, dummy, out.query, out.fragment);
        return out;
    }

    std::string relPath;
    detail::splitPathQueryFrag(ref, relPath, out.query, out.fragment);
    if (!relPath.empty() && relPath.front() == '/') {
        out.path = detail::removeDotSegments(relPath);
    } else {
        std::string merged;
        size_t lastSlash = base->path.rfind('/');
        if (lastSlash != std::string::npos) {
            merged = base->path.substr(0, lastSlash + 1);
        } else {
            merged = "/";
        }
        merged += relPath;
        out.path = detail::removeDotSegments(merged);
    }
    return out;
}

} // namespace shop::core
