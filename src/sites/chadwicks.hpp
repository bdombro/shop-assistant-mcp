#pragma once

#include "site_helpers.hpp"

#include <format>
#include <regex>
#include <string>
#include <vector>

namespace shop::sites {

inline const std::string CHADWICKS_BASE = "https://www.chadwicks.ie/";

inline core::Delivery chadwicksDeliveryFor(std::optional<bool> inStock) {
    if (inStock.has_value() && !*inStock) {
        return core::Delivery{.status = "unavailable", .source = "product"};
    }
    return core::Delivery{
        .status = "available",
        .minDays = 1,
        .maxDays = 2,
        .cost = 29.99,
        .source = "site default"
    };
}

inline std::optional<core::Offer> parseChadwicksJsonRecord(const slimr::Json& rec, const std::string& checkedAt) {
    if (!rec.is_object()) return std::nullopt;
    auto getStr = [&](std::string_view k) -> std::string {
        if (rec.contains(k) && rec[k].is_string()) return rec[k].get<std::string>();
        return "";
    };
    auto getNum = [&](std::string_view k) -> std::optional<double> {
        if (rec.contains(k)) {
            if (rec[k].is_number()) return rec[k].as<double>();
            if (rec[k].is_string()) {
                auto p = core::parsePrice(rec[k].get<std::string>());
                if (p) return *p;
            }
        }
        return std::nullopt;
    };

    std::string name = cleanText(decodeHtmlEntities(getStr("name")));
    std::string url = getStr("url");
    std::string sku = getStr("sku");
    if (sku.empty()) sku = getStr("id");

    auto priceOpt = getNum("price");
    if (!priceOpt) priceOpt = getNum("finalPrice");
    if (!priceOpt || name.empty() || url.empty()) return std::nullopt;

    core::Price p;
    p.amount = *priceOpt;
    p.taxIncluded = true;
    p.currency = "EUR";
    auto oldPrice = getNum("originalPrice");
    if (oldPrice && *oldPrice > p.amount) {
        p.was = *oldPrice;
    }

    std::string imageUrl = getStr("thumbnail");
    if (imageUrl.empty()) imageUrl = getStr("image");

    std::string brand = cleanText(decodeHtmlEntities(getStr("manufacturer")));

    std::map<std::string, slimr::Json> details;
    if (!brand.empty()) details["Brand"] = slimr::Json(brand);
    if (!imageUrl.empty()) details["Image"] = slimr::Json(imageUrl);

    std::string canonical = core::canonicalUrl(url, CHADWICKS_BASE);
    std::string id = sku.empty() ? canonical : sku;

    core::Offer offer;
    offer.site = "chadwicks";
    offer.id = id;
    offer.url = canonical;
    offer.name = name;
    if (!brand.empty()) offer.brand = brand;
    offer.price = p;
    offer.delivery = chadwicksDeliveryFor(std::nullopt);
    offer.details = std::move(details);
    offer.checkedAt = checkedAt;
    return offer;
}

inline core::SearchResult parseChadwicksSearch(std::string_view jsonStr, const std::string& checkedAt) {
    slimr::Json json;
    auto ec = glz::read_json(json, jsonStr);
    if (ec) return core::SearchResult{};
    if (!json.is_object()) return core::SearchResult{};
    const slimr::Json* records = nullptr;
    if (json.contains("result") && json["result"].is_array()) {
        records = &json["result"];
    } else if (json.contains("records") && json["records"].is_array()) {
        records = &json["records"];
    }
    if (!records) return core::SearchResult{};

    std::vector<core::Offer> offers;
    for (const auto& rec : records->get<slimr::Json::array_t>()) {
        auto offerOpt = parseChadwicksJsonRecord(rec, checkedAt);
        if (offerOpt) {
            offers.push_back(*offerOpt);
        }
    }
    return core::SearchResult{.offers = offers, .total = static_cast<int>(offers.size())};
}

inline core::Offer parseChadwicksProduct(std::string_view html, const std::string& url, const std::string& checkedAt) {
    auto doc = core::Document::parse(html);
    auto blocks = core::jsonLdBlocks(html);
    auto ld = core::findLd(blocks, "Product");

    std::string title;
    std::optional<double> priceVal;
    std::string sku;
    std::string brand;
    std::string imageUrl;

    if (ld && ld->is_object()) {
        if ((*ld).contains("name") && (*ld)["name"].is_string()) title = cleanText((*ld)["name"].get<std::string>());
        if ((*ld).contains("sku") && (*ld)["sku"].is_string()) sku = (*ld)["sku"].get<std::string>();
        if ((*ld).contains("brand") && (*ld)["brand"].is_object() && (*ld)["brand"].contains("name")) {
            brand = cleanText((*ld)["brand"]["name"].get<std::string>());
        }
        if ((*ld).contains("image")) {
            if ((*ld)["image"].is_string()) imageUrl = (*ld)["image"].get<std::string>();
            else if ((*ld)["image"].is_array() && !(*ld)["image"].empty() && (*ld)["image"][0].is_string()) {
                imageUrl = (*ld)["image"][0].get<std::string>();
            }
        }
        if ((*ld).contains("offers") && (*ld)["offers"].is_object()) {
            const auto& off = (*ld)["offers"];
            if (off.contains("price") && off["price"].is_number()) priceVal = off["price"].as<double>();
            else if (off.contains("price") && off["price"].is_string()) priceVal = core::parsePrice(off["price"].get<std::string>());
        }
    }

    if (title.empty()) {
        auto h1 = doc.querySelector("h1.page-title span");
        if (!h1) h1 = doc.querySelector("h1");
        if (h1) title = cleanText(h1->text());
    }

    if (!priceVal) {
        auto pSpan = doc.querySelector(".price-final_price .price");
        if (!pSpan) pSpan = doc.querySelector(".special-price .price");
        if (!pSpan) pSpan = doc.querySelector(".price");
        if (pSpan) {
            priceVal = core::parsePrice(pSpan->text());
        }
    }

    core::Price p;
    p.amount = priceVal.value_or(0.0);
    p.taxIncluded = true;
    p.currency = "EUR";

    std::map<std::string, slimr::Json> details;
    if (!brand.empty()) details["Brand"] = slimr::Json(brand);
    if (!sku.empty()) details["SKU"] = slimr::Json(sku);
    if (!imageUrl.empty()) details["Image"] = slimr::Json(imageUrl);

    std::string canonical = core::canonicalUrl(url, CHADWICKS_BASE);
    std::string id = sku.empty() ? canonical : sku;

    core::Offer offer;
    offer.site = "chadwicks";
    offer.id = id;
    offer.url = canonical;
    offer.name = title.empty() ? "Chadwicks Product" : title;
    if (!brand.empty()) offer.brand = brand;
    offer.price = p;
    offer.delivery = chadwicksDeliveryFor(std::nullopt);
    offer.details = std::move(details);
    offer.checkedAt = checkedAt;
    return offer;
}

class ChadwicksAdapter : public core::SiteAdapter {
public:
    std::string id() const override { return "chadwicks"; }
    core::SearchResult search(core::SiteContext& ctx, const core::SearchQuery& q) override {
        std::string url = CHADWICKS_BASE + "catalogsearch/result/?q=" + slimr::url_encode(q.query);
        std::string text = ctx.fetchText(url);
        return parseChadwicksSearch(text, "2026-11-10T00:00:00Z");
    }
    std::vector<core::Offer> offer(core::SiteContext& ctx, const core::OfferRef& ref, const core::OfferOptions&) override {
        std::string url = ref.url.value_or("");
        if (url.empty()) throw std::runtime_error("chadwicks: offer requires url");
        std::string html = ctx.fetchText(core::canonicalUrl(url, CHADWICKS_BASE));
        return {parseChadwicksProduct(html, url, "2026-11-10T00:00:00Z")};
    }
};

} // namespace shop::sites
