#pragma once

#include "core/adapter.hpp"
#include "core/dom.hpp"
#include "curl.hpp"
#include "json.hpp"

#include <algorithm>
#include <cmath>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace shop::sites {

using namespace shop::core;

namespace tooldepotireland_internal {

inline const std::string BASE = "https://www.tooldepotireland.ie/";
inline const std::string API = "https://www.tooldepotireland.ie/wp-json/wc/store/v1/products";
inline const Delivery DELIVERY{
    .status = "available",
    .minDays = 1,
    .maxDays = 1,
    .cost = std::nullopt,
    .source = "site default"
};

struct StockInfo {
    bool inStock = false;
    bool orderIn = false;
    std::optional<int> qty;
    std::string stockText;
};

inline StockInfo stockOf(const slimr::Json& r) {
    std::string t;
    if (r.contains("stock_availability") && r["stock_availability"].is_object() &&
        r["stock_availability"].contains("text") && r["stock_availability"]["text"].is_string()) {
        t = text(r["stock_availability"]["text"].get<std::string>());
    }

    std::optional<int> qty;
    static const std::regex qtyRe(R"(^(\d+) in stock)", std::regex::icase);
    std::smatch m;
    if (std::regex_search(t, m, qtyRe)) {
        qty = std::stoi(m[1].str());
    }

    bool is_in_stock = r.contains("is_in_stock") && r["is_in_stock"].is_boolean() ? r["is_in_stock"].get<bool>() : false;
    bool is_purchasable = !r.contains("is_purchasable") || !r["is_purchasable"].is_boolean() || r["is_purchasable"].get<bool>();

    static const std::regex orderInRe(R"(available to order)", std::regex::icase);
    bool orderIn = std::regex_search(t, orderInRe);

    return StockInfo{
        .inStock = (is_in_stock && is_purchasable),
        .orderIn = orderIn,
        .qty = qty,
        .stockText = std::move(t)
    };
}

inline std::map<std::string, std::string> specsFromTable(std::string_view html) {
    auto doc = Document::parse("<div>" + std::string(html) + "</div>");
    std::map<std::string, std::string> out;
    auto trs = doc.querySelectorAll("tr");
    for (const auto& tr : trs) {
        auto th = tr.querySelector("th");
        auto td = tr.querySelector("td");
        if (th && td) {
            std::string k = text(th->text());
            std::string v = text(td->text());
            if (!k.empty() && !v.empty() && out.find(k) == out.end()) {
                out[k] = v;
            }
        }
    }
    return out;
}

} // namespace tooldepotireland_internal

inline std::optional<Offer> parseTooldepotirelandRow(const slimr::Json& r, std::string_view checkedAt, bool full) {
    using namespace tooldepotireland_internal;
    if (!r.is_object()) return std::nullopt;

    if (!r.contains("prices") || !r["prices"].is_object()) return std::nullopt;
    auto& prices = r["prices"];

    int minor = 2;
    if (prices.contains("currency_minor_unit") && prices["currency_minor_unit"].is_number()) {
        minor = static_cast<int>(prices["currency_minor_unit"].get<double>());
    }

    double priceVal = 0.0;
    if (prices.contains("price") && prices["price"].is_string()) {
        try { priceVal = std::stod(prices["price"].get<std::string>()); } catch (...) {}
    } else if (prices.contains("price") && prices["price"].is_number()) {
        priceVal = prices["price"].get<double>();
    }
    double amount = priceVal / std::pow(10.0, minor);

    std::string permalink;
    if (r.contains("permalink") && r["permalink"].is_string()) {
        permalink = r["permalink"].get<std::string>();
    }
    if (!std::isfinite(amount) || amount <= 0 || permalink.empty()) return std::nullopt;

    double regularVal = 0.0;
    if (prices.contains("regular_price") && prices["regular_price"].is_string()) {
        try { regularVal = std::stod(prices["regular_price"].get<std::string>()); } catch (...) {}
    } else if (prices.contains("regular_price") && prices["regular_price"].is_number()) {
        regularVal = prices["regular_price"].get<double>();
    }
    double regular = regularVal / std::pow(10.0, minor);

    bool on_sale = r.contains("on_sale") && r["on_sale"].is_boolean() ? r["on_sale"].get<bool>() : false;
    auto stock = stockOf(r);

    std::optional<std::string> category;
    if (r.contains("categories") && r["categories"].is_array()) {
        for (const auto& c : r["categories"].get<slimr::Json::array_t>()) {
            if (c.is_object() && c.contains("name") && c["name"].is_string()) {
                std::string catName = text(c["name"].get<std::string>());
                if (!catName.empty() && catName != "Brands") {
                    category = std::move(catName);
                    break;
                }
            }
        }
    }

    std::map<std::string, slimr::Json> details;
    if (r.contains("sku") && r["sku"].is_string()) {
        std::string sku = text(r["sku"].get<std::string>());
        if (!sku.empty()) details["SKU"] = sku;
    }
    if (category) {
        details["Category"] = *category;
    }
    if (!stock.stockText.empty()) {
        details["Availability"] = stock.stockText;
    }

    if (full) {
        if (r.contains("description") && r["description"].is_string()) {
            auto tableSpecs = specsFromTable(r["description"].get<std::string>());
            for (const auto& [k, v] : tableSpecs) {
                details[k] = v;
            }
        }
        if (r.contains("short_description") && r["short_description"].is_string()) {
            auto shortDoc = Document::parse("<div>" + r["short_description"].get<std::string>() + "</div>");
            std::string shortText = shortDoc.documentElement() ? text(shortDoc.documentElement().text()) : "";
            if (!shortText.empty()) {
                details["Summary"] = shortText;
            }
        }
    }

    double ratingVal = 0.0;
    if (r.contains("average_rating") && r["average_rating"].is_string()) {
        try { ratingVal = std::stod(r["average_rating"].get<std::string>()); } catch (...) {}
    } else if (r.contains("average_rating") && r["average_rating"].is_number()) {
        ratingVal = r["average_rating"].get<double>();
    }

    std::optional<int> reviewCount;
    if (r.contains("review_count") && r["review_count"].is_number()) {
        reviewCount = static_cast<int>(r["review_count"].get<double>());
    }

    std::optional<Rating> rating;
    if (ratingVal > 0) {
        rating = Rating{.value = ratingVal, .scale = 5.0, .count = reviewCount};
    }

    std::string source = full ? "product" : "listing";
    Delivery delivery;
    if (!stock.inStock) {
        delivery = Delivery{.status = "unavailable", .source = std::move(source)};
    } else if (stock.orderIn) {
        delivery = Delivery{.status = "available", .minDays = 5, .maxDays = 6, .cost = std::nullopt, .source = std::move(source)};
    } else {
        delivery = DELIVERY;
    }

    std::string id;
    if (r.contains("id") && r["id"].is_number()) {
        id = std::to_string(static_cast<int64_t>(r["id"].get<double>()));
    } else if (r.contains("id") && r["id"].is_string()) {
        id = r["id"].get<std::string>();
    }

    std::string name = r.contains("name") && r["name"].is_string() ? text(r["name"].get<std::string>()) : "";

    std::optional<std::string> brand;
    if (r.contains("brands") && r["brands"].is_array() && !r["brands"].get<slimr::Json::array_t>().empty()) {
        auto& b0 = r["brands"].get<slimr::Json::array_t>()[0];
        if (b0.is_object() && b0.contains("name") && b0["name"].is_string()) {
            brand = text(b0["name"].get<std::string>());
        }
    }

    std::string currencyCode = "EUR";
    if (prices.contains("currency_code") && prices["currency_code"].is_string()) {
        std::string cc = prices["currency_code"].get<std::string>();
        if (!cc.empty()) currencyCode = std::move(cc);
    }

    std::optional<double> was;
    if (on_sale && regular > amount) {
        was = regular;
    }

    Offer o;
    o.site = "tooldepotireland";
    o.id = std::move(id);
    o.url = canonicalUrl(permalink, BASE);
    o.name = std::move(name);
    o.brand = std::move(brand);
    o.category = std::move(category);
    o.price = Price{.amount = amount, .currency = std::move(currencyCode), .was = was, .taxIncluded = true};
    o.inStock = stock.inStock;
    o.rating = std::move(rating);
    o.delivery = std::move(delivery);
    o.details = std::move(details);
    o.checkedAt = std::string(checkedAt);
    return o;
}

inline SearchResult parseTooldepotirelandSearch(const slimr::Json& json, std::string_view checkedAt) {
    std::vector<Offer> offers;
    if (json.is_array()) {
        for (const auto& r : json.get<slimr::Json::array_t>()) {
            auto o = parseTooldepotirelandRow(r, checkedAt, false);
            if (o) offers.push_back(std::move(*o));
        }
    }
    return SearchResult{.offers = std::move(offers)};
}

inline std::vector<Pickup> parseTooldepotirelandStock(const slimr::Json& r, const Near& near) {
    using namespace tooldepotireland_internal;
    auto stock = stockOf(r);
    std::vector<Pickup> result;
    for (const auto& s : near.stores) {
        double km = std::round(s.km * 10.0) / 10.0;
        std::string status;
        if (!stock.inStock || stock.orderIn) {
            status = "out";
        } else if (stock.qty.has_value() && *stock.qty <= 2) {
            status = "low";
        } else {
            status = "in stock";
        }
        std::optional<std::string> readyBy;
        if (stock.orderIn) {
            readyBy = "5-6 working days (ordered in)";
        }
        result.push_back(Pickup{
            .store = s.name,
            .km = km,
            .status = std::move(status),
            .qty = stock.qty,
            .readyBy = std::move(readyBy)
        });
    }
    return result;
}

class ToolDepotIrelandAdapter : public SiteAdapter {
public:
    std::string id() const override { return "tooldepotireland"; }
    std::string matchHint() const override {
        return "The site needs few terms and matches product names only: one or two words, "
               "or a model number (\"drill\", \"makita combi\", \"DCD778\"). Extra words or sizes return nothing.";
    }

    SearchResult search(SiteContext& ctx, const SearchQuery& query) override {
        int perPage = std::min(query.limit * 2, 40);
        std::string url = tooldepotireland_internal::API +
            "?search=" + slimr::url_encode(query.query) +
            "&per_page=" + std::to_string(perPage);
        slimr::Json json = ctx.fetchJson(url);
        return parseTooldepotirelandSearch(json, "");
    }

    std::vector<Offer> offer(SiteContext& ctx, const OfferRef& ref, const OfferOptions& options) override {
        (void)options;
        std::string slug;
        if (ref.url) {
            static const std::regex slugRe(R"(/product/([^/?#]+))");
            std::smatch m;
            if (std::regex_search(*ref.url, m, slugRe)) {
                slug = m[1].str();
            }
        }
        std::string id = ref.id.value_or("");
        if (id.empty() && slug.empty()) {
            throw std::runtime_error("tooldepotireland: give an id or a product URL");
        }

        slimr::Json row;
        if (!id.empty()) {
            row = ctx.fetchJson(tooldepotireland_internal::API + "/" + slimr::url_encode(id));
        } else {
            auto arr = ctx.fetchJson(tooldepotireland_internal::API + "?slug=" + slimr::url_encode(slug));
            if (arr.is_array() && !arr.get<slimr::Json::array_t>().empty()) {
                row = arr.get<slimr::Json::array_t>()[0];
            }
        }

        auto o = parseTooldepotirelandRow(row, "", true);
        if (!o) {
            throw std::runtime_error("tooldepotireland: no product " + (!id.empty() ? id : slug));
        }
        return {*o};
    }

    std::vector<Offer> pickup(SiteContext& ctx, const std::vector<Offer>& offers, const Near& near) override {
        std::vector<Offer> out;
        out.reserve(offers.size());
        for (auto offer : offers) {
            slimr::Json row = ctx.fetchJson(tooldepotireland_internal::API + "/" + slimr::url_encode(offer.id));
            offer.pickup = parseTooldepotirelandStock(row, near);
            out.push_back(std::move(offer));
        }
        return out;
    }
};

} // namespace shop::sites
