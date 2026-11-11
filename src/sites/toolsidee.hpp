#pragma once

#include "core/adapter.hpp"
#include "core/dom.hpp"
#include "curl.hpp"
#include "json.hpp"

#include <algorithm>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace shop::sites {

using namespace shop::core;

namespace toolsidee_internal {

inline const std::string BASE = "https://www.toolsidee.ie/";
inline const Delivery DELIVERY{
    .status = "available",
    .minDays = 1,
    .maxDays = 3,
    .cost = 15.49,
    .source = "site default"
};

inline std::string idOf(std::string_view url) {
    static const std::regex idRe(R"(-(P\d+)\.html)", std::regex::icase);
    std::string s(url);
    std::smatch m;
    if (std::regex_search(s, m, idRe)) {
        return m[1].str();
    }
    return s;
}

} // namespace toolsidee_internal

inline SearchResult parseToolsideeSearch(std::string_view html, std::string_view checkedAt) {
    using namespace toolsidee_internal;
    auto doc = Document::parse(html);
    std::vector<Offer> offers;

    auto lis = doc.querySelectorAll("ul#product_list_show > li.product");
    for (const auto& li : lis) {
        auto a = li.querySelector("a[href*=\"/product/\"]");
        if (!a) continue;
        auto href = a->attr("href");
        if (!href) continue;

        auto priceEl = li.querySelector(".product_price_holder .amount");
        std::optional<double> amount = priceEl ? parsePrice(priceEl->text()) : std::nullopt;
        if (!amount || *amount <= 0) continue;

        std::string url = canonicalUrl(*href, BASE);
        auto h3 = li.querySelector("h3");
        std::string name = h3 ? text(h3->text()) : "";

        auto wasEl = li.querySelector(".advies_prijs_klein s");
        std::optional<double> was = wasEl ? parsePrice(wasEl->text()) : std::nullopt;

        auto stockEl = li.querySelector(".op_voorraad");
        std::string stock = stockEl ? text(stockEl->text()) : "";

        auto leadEl = li.querySelector(".levertijd");
        std::string lead = leadEl ? text(leadEl->text()) : "";

        static const std::regex daysRe(R"((\d+)\s*-\s*(\d+)\s*working days)", std::regex::icase);
        std::smatch dm;
        std::optional<int> minDays, maxDays;
        if (std::regex_search(lead, dm, daysRe)) {
            minDays = std::stoi(dm[1].str());
            maxDays = std::stoi(dm[2].str());
        }

        auto inStock = parseStock(stock);
        auto quantity = parseQuantity(name);

        std::map<std::string, slimr::Json> details;
        if (!stock.empty()) details["Stock"] = stock;
        if (!lead.empty()) details["Delivery time"] = lead;

        auto specsEl = li.querySelector("p.product_specs_l");
        if (specsEl) {
            std::string specs = text(specsEl->text());
            if (!specs.empty()) details["Description"] = specs;
        }

        Delivery delivery;
        if (inStock.has_value() && !*inStock) {
            delivery = Delivery{.status = "unavailable", .source = "listing"};
        } else if (minDays && maxDays) {
            delivery = Delivery{
                .status = "available",
                .minDays = *minDays,
                .maxDays = *maxDays,
                .cost = 15.49,
                .source = "listing"
            };
        } else {
            delivery = DELIVERY;
        }

        Offer o;
        o.site = "toolsidee";
        o.id = idOf(url);
        o.url = std::move(url);
        o.name = std::move(name);
        o.price = Price{
            .amount = *amount,
            .currency = "EUR",
            .was = (was.has_value() && *was > *amount) ? was : std::nullopt,
            .taxIncluded = true
        };
        o.quantity = std::move(quantity);
        o.inStock = inStock;
        o.delivery = std::move(delivery);
        o.details = std::move(details);
        o.checkedAt = std::string(checkedAt);
        offers.push_back(std::move(o));
    }
    return SearchResult{.offers = std::move(offers)};
}

inline Offer parseToolsideeProduct(std::string_view html, std::string_view checkedAt) {
    using namespace toolsidee_internal;
    auto doc = Document::parse(html);
    auto blocks = doc.jsonLd();
    auto productOpt = findLd(blocks, "Product");
    if (!productOpt || !productOpt->is_object()) {
        throw std::runtime_error("toolsidee: no Product JSON-LD on the page (layout changed?)");
    }
    const auto& product = *productOpt;

    if (!product.contains("offers") || !product["offers"].is_object()) {
        throw std::runtime_error("toolsidee: no price in the Product JSON-LD");
    }
    const auto& offerObj = product["offers"];

    std::optional<double> amount;
    if (offerObj.contains("price")) {
        if (offerObj["price"].is_string()) {
            amount = parsePrice(offerObj["price"].get<std::string>());
        } else if (offerObj["price"].is_number()) {
            amount = offerObj["price"].get<double>();
        }
    }
    if (!amount.has_value()) {
        throw std::runtime_error("toolsidee: no price in the Product JSON-LD");
    }

    std::string rawUrl;
    if (product.contains("@id") && product["@id"].is_string()) {
        rawUrl = product["@id"].get<std::string>();
    } else if (product.contains("url") && product["url"].is_string()) {
        rawUrl = product["url"].get<std::string>();
    }
    std::string url = canonicalUrl(rawUrl, BASE);
    size_t hostPos = url.find("://toolsidee.ie");
    if (hostPos != std::string::npos) {
        url.replace(hostPos, 15, "://www.toolsidee.ie");
    }

    std::optional<bool> inStock;
    if (offerObj.contains("availability") && offerObj["availability"].is_string()) {
        std::string avail = offerObj["availability"].get<std::string>();
        static const std::regex schemaRe(R"(^https?://schema\.org/)");
        avail = std::regex_replace(avail, schemaRe, "");
        inStock = parseStock(avail);
    }

    std::map<std::string, slimr::Json> details;
    slimr::Json::array_t features;
    auto ths = doc.querySelectorAll("tr > th");
    static const std::regex kvRe(R"(^([^:]{2,60}):\s*(.+)$)");
    for (const auto& th : ths) {
        std::string row = text(th.text());
        std::smatch m;
        if (std::regex_search(row, m, kvRe)) {
            details[m[1].str()] = m[2].str();
        } else if (!row.empty()) {
            features.push_back(row);
        }
    }
    if (!features.empty()) {
        details["Features"] = slimr::Json(std::move(features));
    }

    if (offerObj.contains("hasMerchantReturnPolicy") && offerObj["hasMerchantReturnPolicy"].is_object()) {
        auto& retObj = offerObj["hasMerchantReturnPolicy"];
        if (retObj.contains("merchantReturnDays") && retObj["merchantReturnDays"].is_number()) {
            details["Returns (days)"] = static_cast<int64_t>(retObj["merchantReturnDays"].get<double>());
        }
    }

    std::string name = product.contains("name") && product["name"].is_string() ? text(product["name"].get<std::string>()) : "";
    auto quantity = parseQuantity(name);

    std::optional<std::string> brand;
    auto bit = details.find("Brand");
    if (bit != details.end() && bit->second.is_string()) {
        brand = bit->second.get<std::string>();
    }

    std::string currency = "EUR";
    if (offerObj.contains("priceCurrency") && offerObj["priceCurrency"].is_string()) {
        std::string pc = text(offerObj["priceCurrency"].get<std::string>());
        if (!pc.empty()) currency = std::move(pc);
    }

    Offer o;
    o.site = "toolsidee";
    o.id = idOf(url);
    o.url = std::move(url);
    o.name = std::move(name);
    o.brand = std::move(brand);
    o.price = Price{.amount = *amount, .currency = std::move(currency), .was = std::nullopt, .taxIncluded = true};
    o.quantity = std::move(quantity);
    o.inStock = inStock;
    if (inStock.has_value() && !*inStock) {
        o.delivery = Delivery{.status = "unavailable", .source = "product"};
    } else {
        o.delivery = DELIVERY;
    }
    o.details = std::move(details);
    o.checkedAt = std::string(checkedAt);
    return o;
}

class ToolsideeAdapter : public SiteAdapter {
public:
    std::string id() const override { return "toolsidee"; }
    bool deliveryOnly() const override { return true; }
    std::string matchHint() const override {
        return "Toolsidee matches loosely on English product titles; try the product type alone.";
    }

    SearchResult search(SiteContext& ctx, const SearchQuery& query) override {
        std::string enc = slimr::url_encode(query.query);
        for (size_t pos = 0; (pos = enc.find("%20", pos)) != std::string::npos; ) {
            enc.replace(pos, 3, "+");
            pos += 1;
        }
        std::string url = toolsidee_internal::BASE + "zoeken/?searchData=" + enc;
        std::string html = ctx.fetchText(url);
        return parseToolsideeSearch(html, "");
    }

    std::vector<Offer> offer(SiteContext& ctx, const OfferRef& ref, const OfferOptions& options) override {
        (void)options;
        std::string url = ref.url.value_or("");
        if (url.empty()) {
            std::string id = ref.id.value_or("");
            url = toolsidee_internal::BASE + "product/item-" + id + ".html";
        }
        std::string html = ctx.fetchText(canonicalUrl(url, toolsidee_internal::BASE));
        return {parseToolsideeProduct(html, "")};
    }
};

} // namespace shop::sites
