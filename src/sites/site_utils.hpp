#pragma once

#include "site_helpers.hpp"

#include <curl/curl.h>
#include <iomanip>
#include <type_traits>

namespace shop::sites {

using namespace shop::core;

template <typename T>
    requires std::is_same_v<std::decay_t<T>, std::optional<std::string>>
inline std::string cleanText(const T& s) {
    return s ? cleanText(std::string_view(*s)) : "";
}

inline std::string cleanText(const Node& node) {
    return node ? cleanText(std::string_view(node.text())) : "";
}

inline std::string cleanText(const std::optional<Node>& node) {
    return (node && *node) ? cleanText(std::string_view(node->text())) : "";
}

inline std::optional<Node> nextElementSibling(const Node& n) {
    auto cur = n.nextSibling();
    while (cur) {
        if (cur->raw() && cur->raw()->type == LXB_DOM_NODE_TYPE_ELEMENT) {
            return cur;
        }
        cur = cur->nextSibling();
    }
    return std::nullopt;
}

inline std::string getQueryParam(std::string_view url, std::string_view key) {
    return getUrlParam(url, key).value_or("");
}

template <typename T>
    requires std::is_same_v<std::decay_t<T>, slimr::Json>
inline std::optional<double> parsePrice(const T& val) {
    if (val.is_number()) {
        return val.get_number();
    }
    if (val.is_string()) {
        return shop::core::parsePrice(val.template get<std::string>());
    }
    return std::nullopt;
}

template <typename T>
    requires std::is_arithmetic_v<std::decay_t<T>>
inline std::optional<double> parsePrice(T d) {
    return static_cast<double>(d);
}

inline std::optional<double> parsePrice(std::string_view s) {
    return shop::core::parsePrice(s);
}

template <typename T>
    requires std::is_same_v<std::decay_t<T>, slimr::Json>
inline std::optional<bool> parseStock(const T& val) {
    if (val.is_boolean()) return val.template get<bool>();
    if (val.is_string()) return shop::core::parseStock(val.template get<std::string>());
    if (val.is_number()) {
        return val.get_number() > 0.0;
    }
    return std::nullopt;
}

inline std::optional<bool> parseStock(std::string_view s) {
    return shop::core::parseStock(s);
}

inline std::string jsonString(const slimr::Json& val, std::string_view def = "") {
    if (val.is_string()) return val.get<std::string>();
    if (val.is_number()) {
        if (val.is_int64()) return std::to_string(val.get<int64_t>());
        std::ostringstream oss;
        oss << val.get_number();
        return oss.str();
    }
    if (val.is_boolean()) return val.get<bool>() ? "true" : "false";
    return std::string(def);
}

inline std::optional<std::string> isoDate(std::string_view label) {
    static const std::regex r1(R"(\bon (\d{1,2}) ([A-Za-z]+) (\d{4}))");
    static const std::regex r2(R"(\bon ([A-Za-z]+) (\d{1,2}), (\d{4}))");
    static const std::vector<std::string> months = {
        "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"
    };

    std::string s(label);
    std::smatch m;
    std::string dayStr, monthStr, yearStr;
    if (std::regex_search(s, m, r1)) {
        dayStr = m[1].str();
        monthStr = m[2].str();
        yearStr = m[3].str();
    } else if (std::regex_search(s, m, r2)) {
        monthStr = m[1].str();
        dayStr = m[2].str();
        yearStr = m[3].str();
    } else {
        return std::nullopt;
    }

    std::string mlow = monthStr.substr(0, 3);
    for (char& c : mlow) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    auto it = std::find(months.begin(), months.end(), mlow);
    if (it == months.end()) return std::nullopt;
    int mNum = static_cast<int>(std::distance(months.begin(), it)) + 1;
    int dNum = std::stoi(dayStr);

    std::ostringstream oss;
    oss << yearStr << "-"
        << std::setw(2) << std::setfill('0') << mNum << "-"
        << std::setw(2) << std::setfill('0') << dNum;
    return oss.str();
}

} // namespace shop::sites
