#pragma once

#include "dom.hpp"
#include "policy.hpp"
#include "db.hpp"
#include "json.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace shop::core {

struct Price {
    double amount = 0.0;
    std::string currency = "EUR";
    std::optional<double> was;
    bool taxIncluded = true;
};

struct Delivery {
    std::string status = "unknown"; // "available" | "unavailable" | "unknown"
    std::optional<int> minDays;
    std::optional<int> maxDays;
    std::optional<double> cost;
    std::string source = "listing"; // "listing" | "product" | "site default"
};

struct Pickup {
    std::string store;
    double km = 0.0;
    std::string status = "unknown"; // "in stock" | "low" | "out" | "unknown"
    std::optional<int> qty;
    std::optional<std::string> readyBy;
};

struct Quantity {
    int count = 1;
    std::optional<std::string> unit;
    std::optional<bool> parsed;
};

struct Rating {
    double value = 0.0;
    double scale = 5.0;
    std::optional<int> count;
    std::optional<std::string> scope; // "variant" | "family"
};

struct Family {
    std::string id;
    int size = 0;
    std::string varies;
};

struct Offer {
    std::string site;
    std::string id;
    std::string url;
    std::string name;
    std::optional<std::string> brand;
    std::optional<std::string> model;
    std::optional<std::string> gtin;
    std::optional<std::string> category;
    std::optional<std::string> seller;
    std::optional<std::string> condition; // "new" | "used" | "refurbished" | "unknown"
    Price price;
    std::optional<Quantity> quantity;
    std::optional<double> unitPrice;
    std::optional<bool> inStock;
    std::optional<Rating> rating;
    Delivery delivery;
    std::optional<std::vector<Pickup>> pickup;
    std::optional<bool> pickupToday;
    std::map<std::string, slimr::Json> details;
    std::optional<int> detailsOmitted;
    std::optional<Family> family;
    std::string checkedAt;
};

struct Review {
    double rating = 0.0;
    double scale = 5.0;
    std::optional<std::string> title;
    std::string text;
    std::optional<std::string> date;
    std::optional<bool> verified;
    std::optional<std::string> variant;
    std::optional<bool> syndicated;
};

struct ProductPage {
    std::string url;
    std::string title;
    std::vector<Offer> offers;
    std::vector<Review> reviews;
};

struct ReviewsResult {
    std::vector<Review> reviews;
    std::optional<int> total;
};

struct StoreStock {
    std::string storeId;
    std::string storeName;
    std::string status = "unknown"; // "in stock" | "low" | "out" | "unknown"
    std::optional<int> qty;
    std::optional<std::string> readyBy;
};

struct PickupResult {
    std::vector<Pickup> branches;
    bool pickupToday = false;
};

struct SearchFilters {
    std::optional<double> minPrice;
    std::optional<double> maxPrice;
    std::optional<bool> inStock;
    std::optional<std::string> brand;
};

struct SearchQuery {
    std::string query;
    SearchFilters filters;
    int limit = 20;
};

struct SearchResult {
    std::vector<Offer> offers;
    std::optional<int> total;
    std::vector<std::string> nativeFilters;
};

struct OfferRef {
    std::optional<std::string> id;
    std::optional<std::string> url;
    std::optional<std::string> seller;
};

struct OfferOptions {
    bool family = false;
    bool sellers = false;
};

struct ReviewQuery {
    int limit = 10;
    std::string sort = "lowest"; // "lowest" | "newest" | "helpful"
};

struct StorePoint {
    std::string id;
    std::string name;
    double lat = 0.0;
    double lon = 0.0;
    double km = 0.0;
};

struct Near {
    double lat = 0.0;
    double lon = 0.0;
    double radiusKm = 0.0;
    std::vector<StorePoint> stores;
};

struct FetchInit {
    std::string method = "GET";
    std::map<std::string, std::string> headers;
    std::optional<std::string> body;
    std::optional<std::string> referrer;
    bool document = false;
};

class SiteContext {
public:
    virtual ~SiteContext() = default;
    virtual void gotoUrl(const std::string& url) = 0;
    virtual std::string fetchText(const std::string& url, const FetchInit& init = {}) = 0;
    virtual slimr::Json fetchJson(const std::string& url, const FetchInit& init = {}) = 0;
    virtual std::string html() = 0;
    virtual void warn(const std::string& message) = 0;
    virtual const SitePolicy& site() const = 0;
    virtual slimr::Json evaluate(const std::string& script) {
        (void)script;
        return {};
    }
};

class SiteAdapter {
public:
    virtual ~SiteAdapter() = default;
    virtual std::string id() const = 0;
    virtual std::string matchHint() const { return ""; }
    virtual bool deliveryOnly() const { return false; }

    virtual SearchResult search(SiteContext& ctx, const SearchQuery& query) = 0;
    virtual std::vector<Offer> offer(SiteContext& ctx, const OfferRef& ref, const OfferOptions& options) = 0;
    virtual std::vector<Offer> pickup(SiteContext& ctx, const std::vector<Offer>& offers, const Near& near) {
        (void)ctx; (void)near;
        return offers;
    }
    virtual ReviewsResult reviews(SiteContext& ctx, const OfferRef& ref, const ReviewQuery& query) {
        (void)ctx; (void)ref; (void)query;
        return {};
    }
};

// Parsing and calculation helpers

inline double distanceKm(double lat1, double lon1, double lat2, double lon2) {
    const double rad = M_PI / 180.0;
    double dLat = (lat2 - lat1) * rad;
    double dLon = (lon2 - lon1) * rad;
    double h = std::sin(dLat / 2.0) * std::sin(dLat / 2.0) +
               std::cos(lat1 * rad) * std::cos(lat2 * rad) *
               std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
    double d = 12742.0 * std::asin(std::sqrt(h));
    return std::round(d * 100.0) / 100.0;
}

inline double distanceKm(const Near& near, double lat2, double lon2) {
    return distanceKm(near.lat, near.lon, lat2, lon2);
}

inline std::string cleanText(std::string_view val) {
    std::string out;
    out.reserve(val.size());
    bool space = false;
    for (char c : val) {
        if (static_cast<unsigned char>(c) <= 32) {
            space = true;
        } else {
            if (space && !out.empty()) {
                out.push_back(' ');
            }
            out.push_back(c);
            space = false;
        }
    }
    return out;
}

inline std::string text(std::string_view val) {
    return cleanText(val);
}

template <typename T>
    requires std::is_same_v<std::decay_t<T>, std::optional<std::string>>
inline std::string text(const T& val) {
    return val ? cleanText(*val) : "";
}

template <typename T>
    requires std::is_same_v<std::decay_t<T>, slimr::Json>
inline std::string text(const T& j) {
    if (j.is_string()) return cleanText(j.template get<std::string>());
    if (j.is_number()) {
        if (j.is_int64()) return std::to_string(j.template get<int64_t>());
        if (j.is_double()) {
            std::string s = std::to_string(j.template get<double>());
            while (s.size() > 1 && s.back() == '0') s.pop_back();
            if (!s.empty() && s.back() == '.') s.pop_back();
            return s;
        }
    }
    return "";
}

inline std::optional<double> parsePrice(std::string_view val) {
    std::string s;
    for (char c : val) {
        if ((c >= '0' && c <= '9') || c == '.' || c == ',') {
            s.push_back(c);
        }
    }
    if (s.empty()) return std::nullopt;
    static const std::regex commaDec(R"(,\d{2}$)");
    if (std::regex_search(s, commaDec)) {
        std::string cleaned;
        for (char c : s) {
            if (c == '.') continue;
            if (c == ',') cleaned.push_back('.');
            else cleaned.push_back(c);
        }
        s = cleaned;
    } else {
        std::string cleaned;
        for (char c : s) {
            if (c != ',') cleaned.push_back(c);
        }
        s = cleaned;
    }
    try {
        size_t idx = 0;
        double d = std::stod(s, &idx);
        if (idx > 0 && std::isfinite(d)) return d;
    } catch (...) {}
    return std::nullopt;
}

inline std::optional<double> parseEuro(std::string_view text) {
    return parsePrice(text);
}

inline std::optional<double> parseGbp(std::string_view text) {
    return parsePrice(text);
}

inline std::optional<bool> parseStock(std::string_view val) {
    if (val.empty()) return std::nullopt;
    std::string s(val);
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    static const std::regex outRe(R"(out ?of ?stock|sold ?out|unavailable|^no$|^false$|discontinued)");
    if (std::regex_search(s, outRe)) return false;

    static const std::regex inRe(R"(in ?stock|^yes$|^true$|available|limited ?stock|^\d+ in stock)");
    if (std::regex_search(s, inRe)) return true;

    return std::nullopt;
}

inline std::optional<Quantity> parseQuantity(std::string_view title) {
    static const std::regex boxRe(R"(\b(?:box|pack|bag|tub) of (\d+)\b)", std::regex::icase);
    static const std::regex pkRe(R"(\b(\d+)\s*-?\s*(?:pk|pack|pcs|pieces|pce|piece)\b)", std::regex::icase);
    std::string s(title);
    std::smatch m;
    if (std::regex_search(s, m, boxRe) || std::regex_search(s, m, pkRe)) {
        int count = std::stoi(m[1].str());
        if (count > 1) {
            Quantity q;
            q.count = count;
            q.parsed = true;
            return q;
        }
    }
    return std::nullopt;
}

inline std::string canonicalUrl(
    std::string_view href,
    std::string_view base,
    const std::vector<std::string>& keep = {}) {
    CURLU* u = curl_url();
    if (!u) return std::string(href);
    if (curl_url_set(u, CURLUPART_URL, std::string(base).c_str(), 0) != CURLUE_OK ||
        curl_url_set(u, CURLUPART_URL, std::string(href).c_str(), 0) != CURLUE_OK) {
        curl_url_cleanup(u);
        return std::string(href);
    }
    curl_url_set(u, CURLUPART_FRAGMENT, nullptr, 0);

    char* query_ptr = nullptr;
    if (curl_url_get(u, CURLUPART_QUERY, &query_ptr, 0) == CURLUE_OK && query_ptr) {
        std::string query = query_ptr;
        curl_free(query_ptr);

        std::vector<std::pair<std::string, std::string>> keptParams;
        std::istringstream iss(query);
        std::string param;
        static const std::regex trackingRe(
            R"(^(utm_.*|gclid|gbraid|wbraid|fbclid|msclkid|srsltid|_ga|ref|ref_|al_click|pos_r|sr|qid|crid|sprefix|pd_rd_.*|pf_rd_.*|content-id)$)",
            std::regex::icase);

        while (std::getline(iss, param, '&')) {
            if (param.empty()) continue;
            size_t eq = param.find('=');
            std::string name = (eq == std::string::npos) ? param : param.substr(0, eq);
            std::string val = (eq == std::string::npos) ? "" : param.substr(eq + 1);
            bool drop = false;
            if (!keep.empty()) {
                drop = (std::find(keep.begin(), keep.end(), name) == keep.end());
            } else {
                drop = val.empty() || std::regex_match(name, trackingRe);
            }
            if (!drop) {
                keptParams.emplace_back(std::move(name), std::move(val));
            }
        }

        if (keptParams.empty()) {
            curl_url_set(u, CURLUPART_QUERY, nullptr, 0);
        } else {
            std::string newQuery;
            for (size_t i = 0; i < keptParams.size(); ++i) {
                if (i > 0) newQuery += "&";
                newQuery += keptParams[i].first;
                if (!keptParams[i].second.empty()) {
                    newQuery += "=" + keptParams[i].second;
                }
            }
            curl_url_set(u, CURLUPART_QUERY, newQuery.c_str(), 0);
        }
    }

    char* full = nullptr;
    std::string result = std::string(href);
    if (curl_url_get(u, CURLUPART_URL, &full, 0) == CURLUE_OK && full) {
        result = full;
        curl_free(full);
    }
    curl_url_cleanup(u);
    return result;
}

inline void capDetails(Offer& offer) {
    if (offer.details.empty()) return;
    const size_t MAX_KEYS = 8;
    const size_t MAX_CHARS = 200;

    auto capVal = [&](auto& self, slimr::Json val) -> slimr::Json {
        if (val.is_string()) {
            std::string s = val.get<std::string>();
            if (s.size() > MAX_CHARS) {
                s = s.substr(0, MAX_CHARS) + "\xE2\x80\xA6";
            }
            return slimr::Json(s);
        }
        if (val.is_array()) {
            slimr::Json::array_t arr;
            size_t limit = std::min<size_t>(val.get<slimr::Json::array_t>().size(), 10);
            for (size_t i = 0; i < limit; ++i) {
                arr.push_back(self(self, val.get<slimr::Json::array_t>()[i]));
            }
            return slimr::Json(std::move(arr));
        }
        if (val.is_object()) {
            slimr::Json::object_t obj;
            size_t count = 0;
            for (const auto& [k, v] : val.get<slimr::Json::object_t>()) {
                if (++count > MAX_KEYS) break;
                obj[k] = self(self, v);
            }
            return slimr::Json(std::move(obj));
        }
        return val;
    };

    std::map<std::string, slimr::Json> kept;
    size_t count = 0;
    size_t total = offer.details.size();
    for (const auto& [k, v] : offer.details) {
        if (++count > MAX_KEYS) break;
        kept[k] = capVal(capVal, v);
    }
    if (total > kept.size()) {
        offer.detailsOmitted = static_cast<int>(total - kept.size());
    }
    offer.details = std::move(kept);
}

struct FitBudgetResult {
    std::vector<Offer> offers;
    std::map<std::string, int> trimmed;
};

inline FitBudgetResult fitToBudget(const std::vector<Offer>& offers, size_t maxChars) {
    std::vector<size_t> sizes;
    sizes.reserve(offers.size());
    for (const auto& o : offers) {
        sizes.push_back(slimr::write_json(o).size());
    }

    std::map<std::string, std::vector<size_t>> bySite;
    for (size_t i = 0; i < offers.size(); ++i) {
        bySite[offers[i].site].push_back(i);
    }

    auto total = [&](const std::vector<size_t>& idxs) -> size_t {
        size_t sum = 0;
        for (size_t idx : idxs) sum += sizes[idx];
        return sum;
    };

    std::vector<std::pair<std::string, std::vector<size_t>>> sortedSites(bySite.begin(), bySite.end());
    std::sort(sortedSites.begin(), sortedSites.end(), [&](const auto& a, const auto& b) {
        return total(a.second) < total(b.second);
    });

    std::vector<bool> keep(offers.size(), false);
    std::map<std::string, int> trimmed;
    double left = static_cast<double>(maxChars);
    size_t sitesLeft = sortedSites.size();

    for (const auto& [site, idxs] : sortedSites) {
        double share = left / static_cast<double>(sitesLeft);
        double used = 0;
        size_t kept = 0;
        for (size_t idx : idxs) {
            if (kept > 0 && used + static_cast<double>(sizes[idx]) > share) break;
            used += static_cast<double>(sizes[idx]);
            kept++;
            keep[idx] = true;
        }
        if (kept < idxs.size()) {
            trimmed[site] = static_cast<int>(idxs.size() - kept);
        }
        left -= used;
        sitesLeft--;
    }

    FitBudgetResult res;
    for (size_t i = 0; i < offers.size(); ++i) {
        if (keep[i]) {
            res.offers.push_back(offers[i]);
        }
    }
    res.trimmed = std::move(trimmed);
    return res;
}

struct ApplyFiltersResult {
    std::vector<Offer> offers;
    int dropped = 0;
    int otherCurrency = 0;
};

inline ApplyFiltersResult applyFilters(
    const std::vector<Offer>& offers,
    const SearchFilters& filters,
    const std::vector<std::string>& native = {},
    const std::string& currency = "") {
    auto want = [&](std::string_view key) -> bool {
        if (key == "minPrice") return filters.minPrice.has_value() && std::find(native.begin(), native.end(), key) == native.end();
        if (key == "maxPrice") return filters.maxPrice.has_value() && std::find(native.begin(), native.end(), key) == native.end();
        if (key == "inStock") return filters.inStock.has_value() && std::find(native.begin(), native.end(), key) == native.end();
        if (key == "brand") return filters.brand.has_value() && std::find(native.begin(), native.end(), key) == native.end();
        return false;
    };

    auto priced = [&](const Offer& o) -> bool {
        return currency.empty() || o.price.currency == currency;
    };

    int otherCurrencyCount = 0;
    if (want("minPrice") || want("maxPrice")) {
        for (const auto& o : offers) {
            if (!priced(o)) otherCurrencyCount++;
        }
    }

    std::vector<Offer> kept;
    for (auto o : offers) {
        if (want("minPrice") && priced(o) && o.price.amount < *filters.minPrice) continue;
        if (want("maxPrice") && priced(o) && o.price.amount > *filters.maxPrice) continue;
        if (want("inStock") && filters.inStock.has_value() && *filters.inStock && o.inStock.has_value() && !*o.inStock) continue;
        if (want("brand")) {
            std::string b = *filters.brand;
            for (char& c : b) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            std::string ob = o.brand.value_or("");
            for (char& c : ob) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            std::string on = o.name;
            for (char& c : on) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (ob.find(b) == std::string::npos && on.find(b) == std::string::npos) continue;
        }

        if (o.quantity.has_value() && o.quantity->count > 1) {
            o.unitPrice = std::round((o.price.amount / static_cast<double>(o.quantity->count)) * 100.0) / 100.0;
        }
        kept.push_back(std::move(o));
    }

    ApplyFiltersResult res;
    res.dropped = static_cast<int>(offers.size() - kept.size());
    res.offers = std::move(kept);
    res.otherCurrency = otherCurrencyCount;
    return res;
}

inline std::string homeUrl(const SitePolicy& site) {
    auto searchIt = site.urls.find("search");
    std::vector<std::string> templates;
    if (searchIt != site.urls.end()) templates.push_back(searchIt->second);
    for (const auto& [_, tmpl] : site.urls) {
        templates.push_back(tmpl);
    }
    for (const auto& tmpl : templates) {
        if (tmpl.empty()) continue;
        std::string s = tmpl;
        static const std::regex placeholderRe(R"(\{[^}]+\})");
        s = std::regex_replace(s, placeholderRe, "x");
        if (siteForUrl(s, {site}) != nullptr) {
            CURLU* u = curl_url();
            if (u) {
                if (curl_url_set(u, CURLUPART_URL, s.c_str(), 0) == CURLUE_OK) {
                    char* scheme = nullptr;
                    char* host = nullptr;
                    char* port = nullptr;
                    curl_url_get(u, CURLUPART_SCHEME, &scheme, 0);
                    curl_url_get(u, CURLUPART_HOST, &host, 0);
                    curl_url_get(u, CURLUPART_PORT, &port, 0);
                    std::string origin = (scheme ? scheme : "https") + std::string("://") + (host ? host : "");
                    if (port) origin += ":" + std::string(port);
                    origin += "/";
                    curl_free(scheme);
                    curl_free(host);
                    curl_free(port);
                    curl_url_cleanup(u);
                    return origin;
                }
                curl_url_cleanup(u);
            }
        }
    }
    if (!site.domains.empty()) {
        return "https://www." + site.domains.front() + "/";
    }
    return "https://localhost/";
}

inline std::string blockedPostsHint(Store& store, int64_t pageId, int64_t sinceMs) {
    auto events = store.guardEvents({pageId}, sinceMs - 1);
    static const std::regex noiseHost(
        R"(google-analytics|googletagmanager|doubleclick|facebook\.com|clarity\.ms|hotjar|sentry\.io|newrelic|datadog|onetrust|cookielaw|criteo|tiktok|klaviyo|bing\.com\/bat)",
        std::regex::icase);

    std::vector<std::string> urls;
    for (const auto& g : events) {
        if (g.outcome == "blocked" && g.method != "GET" && !std::regex_search(g.url, noiseHost)) {
            std::string formatted = g.url;
            CURLU* u = curl_url();
            if (u) {
                if (curl_url_set(u, CURLUPART_URL, g.url.c_str(), 0) == CURLUE_OK) {
                    char* host = nullptr;
                    char* path = nullptr;
                    curl_url_get(u, CURLUPART_HOST, &host, 0);
                    curl_url_get(u, CURLUPART_PATH, &path, 0);
                    if (host && path) {
                        formatted = std::string(host) + std::string(path);
                    }
                    curl_free(host);
                    curl_free(path);
                }
                curl_url_cleanup(u);
            }
            if (std::find(urls.begin(), urls.end(), formatted) == urls.end()) {
                urls.push_back(std::move(formatted));
            }
        }
    }
    if (urls.size() > 5) urls.resize(5);
    if (!urls.empty()) {
        std::string res = " (blocked POSTs: ";
        for (size_t i = 0; i < urls.size(); ++i) {
            if (i > 0) res += ", ";
            res += urls[i];
        }
        res += "; consider adding challenge endpoints to `readPosts`)";
        return res;
    }
    return "";
}

class AdapterRegistry {
public:
    static AdapterRegistry& instance() {
        static AdapterRegistry reg;
        return reg;
    }

    void registerAdapter(std::unique_ptr<SiteAdapter> adapter) {
        std::string id = adapter->id();
        adapters_[std::move(id)] = std::move(adapter);
    }

    SiteAdapter* get(std::string_view id) const {
        auto it = adapters_.find(std::string(id));
        if (it != adapters_.end()) return it->second.get();
        return nullptr;
    }

    std::vector<std::string> allIds() const {
        std::vector<std::string> ids;
        for (const auto& [k, _] : adapters_) ids.push_back(k);
        return ids;
    }

private:
    AdapterRegistry() = default;
    std::map<std::string, std::unique_ptr<SiteAdapter>> adapters_;
};

} // namespace shop::core
