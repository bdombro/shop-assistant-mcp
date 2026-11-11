#pragma once

#include "site_helpers.hpp"

#include <chrono>
#include <format>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

namespace shop::sites::duckduckgo_ns {

struct MerchantUrlResult {
    std::string url;
    std::string host;
};

inline std::string cleanMerchantUrl(std::string_view href) {
    static const std::regex trackingParamRe(
        R"(^(utm_.*|chn|ship_to|country|currency|msockid|msclkid|adp|mark|biz|searchText|fclid|source|gclid|aff_.*|sk|spm|ref_?|click_src|user|feed|psc|tag|algo_.*|pdp_.*)$)",
        std::regex::icase);

    std::string_view urlWithoutFrag = href;
    size_t hashPos = urlWithoutFrag.find('#');
    if (hashPos != std::string_view::npos) {
        urlWithoutFrag = urlWithoutFrag.substr(0, hashPos);
    }

    size_t qpos = urlWithoutFrag.find('?');
    if (qpos == std::string_view::npos) {
        return std::string(urlWithoutFrag);
    }

    std::string base(urlWithoutFrag.substr(0, qpos));
    std::string_view query = urlWithoutFrag.substr(qpos + 1);
    std::vector<std::string> kept;

    while (!query.empty()) {
        size_t amp = query.find('&');
        std::string_view pair = (amp == std::string_view::npos) ? query : query.substr(0, amp);
        if (!pair.empty()) {
            size_t eq = pair.find('=');
            std::string key(eq == std::string_view::npos ? pair : pair.substr(0, eq));
            if (!std::regex_match(key, trackingParamRe)) {
                kept.emplace_back(pair);
            }
        }
        if (amp == std::string_view::npos) break;
        query = query.substr(amp + 1);
    }

    if (kept.empty()) {
        return base;
    }
    std::string res = base + "?";
    for (size_t i = 0; i < kept.size(); ++i) {
        if (i > 0) res += "&";
        res += kept[i];
    }
    return res;
}

inline std::optional<MerchantUrlResult> merchantUrl(std::string_view tracker) {
    auto spldStr = getUrlParam(tracker, "spld");
    if (!spldStr) return std::nullopt;
    auto spld = slimr::parse_json<slimr::Json>(*spldStr);
    if (!spld || !spld->is_object() || !spld->contains("u")) return std::nullopt;
    std::string u = (*spld)["u"].get<std::string>();
    if (u.empty()) return std::nullopt;

    std::string targetUrl;
    if (u.starts_with("a1")) {
        std::string wrapped = base64UrlDecode(u.substr(2));
        if (wrapped.starts_with("http")) {
            auto innerUrl = getUrlParam(wrapped, "url");
            if (innerUrl) targetUrl = *innerUrl;
        } else {
            targetUrl = urlDecode(base64UrlDecode(u));
        }
    } else {
        targetUrl = urlDecode(base64UrlDecode(u));
    }

    if (targetUrl.empty()) return std::nullopt;

    std::string host = getUrlHost(targetUrl);
    static const std::regex redirectHostRe(R"(s\.click\.aliexpress\.com|delupe\.ie)");
    if (std::regex_search(host, redirectHostRe)) {
        auto dl = getUrlParam(targetUrl, "dl_target_url");
        if (!dl) dl = getUrlParam(targetUrl, "uri");
        if (dl && !dl->empty()) {
            targetUrl = *dl;
            while (targetUrl.find("http%3A") != std::string::npos || targetUrl.find("http%3a") != std::string::npos ||
                   targetUrl.find("https%3A") != std::string::npos || targetUrl.find("https%3a") != std::string::npos) {
                targetUrl = urlDecode(targetUrl);
            }
        }
    }

    std::string clean = cleanMerchantUrl(targetUrl);
    std::string cleanHost = getUrlHost(clean);
    if (cleanHost.starts_with("www.")) {
        cleanHost = cleanHost.substr(4);
    }
    return MerchantUrlResult{clean, cleanHost};
}

inline std::optional<std::string> parseVqd(std::string_view html) {
    static const std::regex vqdRe(R"(vqd[=:'"\s]+([\d-]{20,}))");
    std::cmatch m;
    if (std::regex_search(html.data(), html.data() + html.size(), m, vqdRe)) {
        return m[1].str();
    }
    return std::nullopt;
}

struct PriceTextResult {
    double amount = 0.0;
    std::string currency;
};

inline std::optional<PriceTextResult> parsePriceText(std::string_view raw) {
    std::string s = cleanText(raw);
    auto amount = core::parsePrice(s);
    if (s.empty() || !amount) return std::nullopt;
    std::string currency;
    if (s.find("€") != std::string::npos) currency = "EUR";
    else if (s.find("£") != std::string::npos) currency = "GBP";
    else if (s.find("$") != std::string::npos) currency = "USD";
    else return std::nullopt;
    return PriceTextResult{*amount, currency};
}

inline std::string slugWords(std::string_view url) {
    size_t qpos = url.find('?');
    std::string_view path = (qpos != std::string_view::npos) ? url.substr(0, qpos) : url;
    size_t scheme = path.find("://");
    if (scheme != std::string_view::npos) {
        size_t slash = path.find('/', scheme + 3);
        if (slash != std::string_view::npos) path = path.substr(slash);
        else path = "";
    }
    std::string last;
    size_t start = 0;
    while (start < path.size()) {
        size_t next = path.find('/', start);
        std::string_view seg = (next == std::string_view::npos) ? path.substr(start) : path.substr(start, next - start);
        if (!seg.empty()) last = std::string(seg);
        if (next == std::string_view::npos) break;
        start = next + 1;
    }
    static const std::regex extRe(R"(\.\w{2,5}$)");
    last = std::regex_replace(last, extRe, "");
    static const std::regex splitRe(R"([-_+\s]+)");
    std::sregex_token_iterator it(last.begin(), last.end(), splitRe, -1);
    std::sregex_token_iterator end;
    std::vector<std::string> words;
    static const std::regex hexHashRe(R"(^(?=.*\d)(?=.*[a-z])[a-z\d]{8,}$)", std::regex::icase);
    for (; it != end; ++it) {
        std::string w = it->str();
        if (w.size() > 1 && !std::regex_match(w, hexHashRe)) {
            words.push_back(w);
            if (words.size() == 8) break;
        }
    }
    if (!words.empty()) {
        std::string res;
        for (size_t i = 0; i < words.size(); ++i) {
            if (i > 0) res += " ";
            res += words[i];
        }
        return res;
    }
    return getUrlHost(url);
}

inline std::vector<core::Offer> parseSearch(const slimr::Json& json, const std::string& checkedAt) {
    std::vector<core::Offer> offers;
    if (!json.is_object() || !json.contains("results") || !json["results"].is_array()) {
        return offers;
    }
    for (const auto& row : json["results"].get<slimr::Json::array_t>()) {
        if (!row.is_object()) continue;
        std::string rawUrl = row.contains("url") && row["url"].is_string() ? row["url"].get<std::string>() : "";
        auto merchant = merchantUrl(rawUrl);
        std::string lowPriceStr = row.contains("low_price") && row["low_price"].is_string() ? row["low_price"].get<std::string>() : "";
        std::string priceStr = row.contains("price") && row["price"].is_string() ? row["price"].get<std::string>() : "";
        auto price = parsePriceText(lowPriceStr);
        if (!price) price = parsePriceText(priceStr);
        std::optional<PriceTextResult> was;
        if (!lowPriceStr.empty()) {
            was = parsePriceText(priceStr);
        }
        if (!merchant || !price) continue;

        std::map<std::string, slimr::Json> details;
        details["Merchant"] = slimr::Json(merchant->host);
        if (row.contains("reviews") && row["reviews"].is_number()) {
            auto rev = row["reviews"].as<int64_t>();
            if (rev > 0) {
                details["Reviews"] = slimr::Json(rev);
            }
        }
        if (row.contains("abstract") && row["abstract"].is_string()) {
            std::string abs = cleanText(row["abstract"].get<std::string>());
            if (!abs.empty()) {
                details["Description"] = slimr::Json(abs);
            }
        }

        core::Offer offer;
        offer.site = "duckduckgo";
        offer.id = merchant->url;
        offer.url = merchant->url;
        offer.name = row.contains("heading") && row["heading"].is_string() ? cleanText(row["heading"].get<std::string>()) : "";
        std::string brand = row.contains("brand") && row["brand"].is_string() ? cleanText(row["brand"].get<std::string>()) : "";
        offer.seller = brand.empty() ? "unknown" : brand;
        offer.condition = "unknown";
        offer.price.amount = price->amount;
        offer.price.currency = price->currency;
        offer.price.taxIncluded = true;
        if (was && was->amount > price->amount) {
            offer.price.was = was->amount;
        }

        if (row.contains("rating") && row["rating"].is_number()) {
            double rval = row["rating"].as<double>();
            if (rval > 0.0) {
                core::Rating rating;
                rating.value = rval;
                rating.scale = 5.0;
                if (row.contains("reviews") && row["reviews"].is_number()) {
                    auto cnt = row["reviews"].as<int>();
                    if (cnt > 0) rating.count = cnt;
                }
                offer.rating = rating;
            }
        }

        offer.delivery = core::Delivery{.status = "unknown", .source = "listing"};
        offer.details = std::move(details);
        offer.checkedAt = checkedAt;

        offers.push_back(std::move(offer));
    }
    return offers;
}

class DuckDuckGo : public core::SiteAdapter {
public:
    std::string id() const override { return "duckduckgo"; }
    bool deliveryOnly() const override { return true; }
    std::string matchHint() const override {
        return "DuckDuckGo Shopping (Bing's feed) matches loosely and returns many unrelated items; use the product's brand and model, and check each row's merchant.";
    }

    core::SearchResult search(core::SiteContext& ctx, const core::SearchQuery& q) override {
        std::string checkedAt = "2026-11-10T00:00:00Z";
        std::string home = ctx.fetchText("https://duckduckgo.com/?q=" + urlEncode(q.query) + "&ia=shopping&iax=shopping");
        auto vqd = parseVqd(home);
        if (!vqd) {
            throw std::runtime_error("duckduckgo: no vqd token on the page (the shopping API changed?)");
        }
        std::string url = "https://links.duckduckgo.com/m.js?shopping=1&o=json&sfexp=1&mkt=ie-en&a=&q=" +
                          urlEncode(q.query) + "&vqd=" + *vqd;
        std::vector<core::Offer> offers;
        const int MAX_PAGES = 3;
        for (int page = 0; !url.empty() && page < MAX_PAGES && static_cast<int>(offers.size()) < q.limit; ++page) {
            slimr::Json json = ctx.fetchJson(url);
            bool hasResults = json.is_object() && json.contains("results") &&
                              json["results"].is_array() && !json["results"].get<slimr::Json::array_t>().empty();
            if (!hasResults && page == 0) {
                ctx.warn("duckduckgo: empty shopping response");
            }
            auto pageOffers = parseSearch(json, checkedAt);
            for (auto& o : pageOffers) {
                offers.push_back(std::move(o));
            }
            if (json.is_object() && json.contains("next") && json["next"].is_string()) {
                url = json["next"].get<std::string>();
            } else {
                url.clear();
            }
        }

        std::vector<core::Offer> deduped;
        for (auto& o : offers) {
            bool found = false;
            for (const auto& existing : deduped) {
                if (existing.id == o.id) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                deduped.push_back(std::move(o));
            }
        }
        return core::SearchResult{.offers = std::move(deduped)};
    }

    std::vector<core::Offer> offer(core::SiteContext& ctx, const core::OfferRef& ref, const core::OfferOptions& options) override {
        (void)options;
        std::string raw = ref.id.value_or(ref.url.value_or(""));
        static const std::regex httpRe(R"(^https?://)");
        if (!std::regex_search(raw, httpRe)) {
            throw std::runtime_error("duckduckgo: pass a merchant product URL as the id");
        }
        std::string clean = cleanMerchantUrl(raw);
        auto sr = search(ctx, core::SearchQuery{.query = slugWords(clean), .limit = 30});
        for (const auto& o : sr.offers) {
            if (o.id == clean) {
                return {o};
            }
        }
        throw std::runtime_error("duckduckgo: " + getUrlHost(clean) +
                                 " listing not found again (no product page on DuckDuckGo): re-check on the merchant's site");
    }
};

} // namespace shop::sites::duckduckgo_ns
namespace shop::sites {
using DuckDuckGo = duckduckgo_ns::DuckDuckGo;
using DuckDuckGoAdapter = duckduckgo_ns::DuckDuckGo;
} // namespace shop::sites
