#pragma once

#include "site_helpers.hpp"

#include <format>
#include <regex>
#include <string>
#include <vector>

namespace shop::sites {

inline const std::string DIY_BASE = "https://www.diy.ie/";

inline core::Delivery diyDeliveryFor(std::optional<bool> inStock) {
    if (inStock.has_value() && !*inStock) {
        return core::Delivery{.status = "unavailable", .source = "product"};
    }
    return core::Delivery{
        .status = "available",
        .minDays = 2,
        .maxDays = 4,
        .cost = 6.0,
        .source = "site default"
    };
}

inline core::SearchResult parseDiySearch(std::string_view html, const std::string& checkedAt) {
    auto doc = core::Document::parse(html);
    std::vector<core::Offer> offers;

    auto cards = doc.querySelectorAll("a.product-card, div.product-card, li.product-list-item");
    for (const auto& card : cards) {
        auto a = (card.tagName() == "a") ? card : *card.querySelector("a");
        std::string href;
        if (auto h = a.attr("href")) href = *h;
        if (href.empty()) continue;

        auto titleEl = card.querySelector(".product-card-title, .title, h3, h2");
        std::string title = titleEl ? cleanText(titleEl->text()) : cleanText(a.text());
        if (title.empty()) continue;

        auto priceEl = card.querySelector(".price, .product-card-price, span[data-test='price']");
        if (!priceEl) continue;
        auto priceVal = core::parsePrice(priceEl->text());
        if (!priceVal) continue;

        core::Price p;
        p.amount = *priceVal;
        p.taxIncluded = true;
        p.currency = "EUR";

        std::string canonical = core::canonicalUrl(href, DIY_BASE);
        std::string id = canonical;

        auto imgEl = card.querySelector("img");
        std::optional<std::string> imageUrl;
        if (imgEl) {
            auto src = imgEl->attr("src");
            if (src) imageUrl = *src;
        }

        std::map<std::string, slimr::Json> details;
        if (imageUrl) details["Image"] = slimr::Json(*imageUrl);

        core::Offer offer;
        offer.site = "diy";
        offer.id = id;
        offer.url = canonical;
        offer.name = title;
        offer.price = p;
        offer.delivery = diyDeliveryFor(std::nullopt);
        offer.details = std::move(details);
        offer.checkedAt = checkedAt;

        offers.push_back(std::move(offer));
    }

    return core::SearchResult{.offers = offers, .total = static_cast<int>(offers.size())};
}

inline core::Offer parseDiyProduct(std::string_view html, const std::string& url, const std::string& checkedAt) {
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
        auto h1 = doc.querySelector("h1");
        if (h1) title = cleanText(h1->text());
    }

    if (!priceVal) {
        auto pEl = doc.querySelector(".product-price, .price");
        if (pEl) priceVal = core::parsePrice(pEl->text());
    }

    core::Price p;
    p.amount = priceVal.value_or(0.0);
    p.taxIncluded = true;
    p.currency = "EUR";

    std::string canonical = core::canonicalUrl(url, DIY_BASE);
    std::string id = sku.empty() ? canonical : sku;

    std::map<std::string, slimr::Json> details;
    if (!brand.empty()) details["Brand"] = slimr::Json(brand);
    if (!sku.empty()) details["SKU"] = slimr::Json(sku);
    if (!imageUrl.empty()) details["Image"] = slimr::Json(imageUrl);

    core::Offer offer;
    offer.site = "diy";
    offer.id = id;
    offer.url = canonical;
    offer.name = title.empty() ? "DIY.ie Product" : title;
    if (!brand.empty()) offer.brand = brand;
    offer.price = p;
    offer.delivery = diyDeliveryFor(std::nullopt);
    offer.details = std::move(details);
    offer.checkedAt = checkedAt;
    return offer;
}

class DiyAdapter : public core::SiteAdapter {
public:
    std::string id() const override { return "diy"; }
    core::SearchResult search(core::SiteContext& ctx, const core::SearchQuery& q) override {
        std::string url = DIY_BASE + "search?term=" + slimr::url_encode(q.query);
        std::string html = ctx.fetchText(url);
        return parseDiySearch(html, "2026-11-10T00:00:00Z");
    }
    std::vector<core::Offer> offer(core::SiteContext& ctx, const core::OfferRef& ref, const core::OfferOptions&) override {
        std::string url = ref.url.value_or("");
        if (url.empty()) throw std::runtime_error("diy: offer requires url");
        std::string html = ctx.fetchText(core::canonicalUrl(url, DIY_BASE));
        return {parseDiyProduct(html, url, "2026-11-10T00:00:00Z")};
    }
};

} // namespace shop::sites
