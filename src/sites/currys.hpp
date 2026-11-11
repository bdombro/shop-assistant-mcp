#pragma once

#include "site_helpers.hpp"

#include <format>
#include <regex>
#include <string>
#include <vector>

namespace shop::sites {

inline const std::string CURRYS_BASE = "https://www.currys.ie/";

inline core::Delivery currysDeliveryFor(std::optional<bool> inStock) {
    if (inStock.has_value() && !*inStock) {
        return core::Delivery{.status = "unavailable", .source = "product"};
    }
    return core::Delivery{
        .status = "available",
        .minDays = 3,
        .maxDays = 5,
        .cost = 5.99,
        .source = "site default"
    };
}

inline core::SearchResult parseCurrysSearch(std::string_view html, const std::string& checkedAt) {
    auto doc = core::Document::parse(html);
    std::vector<core::Offer> offers;

    auto tiles = doc.querySelectorAll(".product-tile, .product-grid-item, div.product");
    for (const auto& tile : tiles) {
        auto a = tile.querySelector("a.product-title, a.title, h3 a, a");
        if (!a) continue;
        auto hrefOpt = a->attr("href");
        if (!hrefOpt) continue;
        std::string href = *hrefOpt;
        std::string title = cleanText(a->text());
        if (title.empty()) {
            auto img = tile.querySelector("img");
            if (img) {
                auto alt = img->attr("alt");
                if (alt) title = cleanText(*alt);
            }
        }
        if (title.empty()) continue;

        auto priceEl = tile.querySelector(".price, .current-price, .value");
        if (!priceEl) continue;
        auto priceVal = core::parsePrice(priceEl->text());
        if (!priceVal) continue;

        core::Price p;
        p.amount = *priceVal;
        p.taxIncluded = true;
        p.currency = "EUR";

        auto wasEl = tile.querySelector(".was-price, .strike, .original-price");
        if (wasEl) {
            auto wasVal = core::parsePrice(wasEl->text());
            if (wasVal && *wasVal > p.amount) p.was = *wasVal;
        }

        std::string canonical = core::canonicalUrl(href, CURRYS_BASE);
        std::string id = canonical;

        auto imgEl = tile.querySelector("img");
        std::optional<std::string> imageUrl;
        if (imgEl) {
            auto src = imgEl->attr("data-src");
            if (!src) src = imgEl->attr("src");
            if (src) imageUrl = *src;
        }

        std::map<std::string, slimr::Json> details;
        if (imageUrl) details["Image"] = slimr::Json(*imageUrl);

        core::Offer offer;
        offer.site = "currys";
        offer.id = id;
        offer.url = canonical;
        offer.name = title;
        offer.price = p;
        offer.delivery = currysDeliveryFor(std::nullopt);
        offer.details = std::move(details);
        offer.checkedAt = checkedAt;

        offers.push_back(std::move(offer));
    }

    return core::SearchResult{.offers = offers, .total = static_cast<int>(offers.size())};
}

inline core::Offer parseCurrysProduct(std::string_view html, const std::string& url, const std::string& checkedAt) {
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
        auto h1 = doc.querySelector("h1.product-title, h1");
        if (h1) title = cleanText(h1->text());
    }

    if (!priceVal) {
        auto pEl = doc.querySelector(".product-price, .price, span[itemprop=\"price\"]");
        if (pEl) priceVal = core::parsePrice(pEl->text());
    }

    core::Price p;
    p.amount = priceVal.value_or(0.0);
    p.taxIncluded = true;
    p.currency = "EUR";

    std::string canonical = core::canonicalUrl(url, CURRYS_BASE);
    std::string id = sku.empty() ? canonical : sku;

    std::map<std::string, slimr::Json> details;
    if (!brand.empty()) details["Brand"] = slimr::Json(brand);
    if (!sku.empty()) details["SKU"] = slimr::Json(sku);
    if (!imageUrl.empty()) details["Image"] = slimr::Json(imageUrl);

    core::Offer offer;
    offer.site = "currys";
    offer.id = id;
    offer.url = canonical;
    offer.name = title.empty() ? "Currys Product" : title;
    if (!brand.empty()) offer.brand = brand;
    offer.price = p;
    offer.delivery = currysDeliveryFor(std::nullopt);
    offer.details = std::move(details);
    offer.checkedAt = checkedAt;
    return offer;
}

class CurrysAdapter : public core::SiteAdapter {
public:
    std::string id() const override { return "currys"; }
    core::SearchResult search(core::SiteContext& ctx, const core::SearchQuery& q) override {
        std::string url = CURRYS_BASE + "search?q=" + slimr::url_encode(q.query);
        std::string html = ctx.fetchText(url);
        return parseCurrysSearch(html, "2026-11-10T00:00:00Z");
    }
    std::vector<core::Offer> offer(core::SiteContext& ctx, const core::OfferRef& ref, const core::OfferOptions&) override {
        std::string url = ref.url.value_or("");
        if (url.empty()) throw std::runtime_error("currys: offer requires url");
        std::string html = ctx.fetchText(core::canonicalUrl(url, CURRYS_BASE));
        return {parseCurrysProduct(html, url, "2026-11-10T00:00:00Z")};
    }
};

} // namespace shop::sites
