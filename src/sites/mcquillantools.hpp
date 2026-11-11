#pragma once

#include "site_helpers.hpp"

#include <format>
#include <regex>
#include <string>
#include <vector>

namespace shop::sites {

inline const std::string MCQUILLAN_BASE = "https://www.mcquillantools.ie/";

inline core::Delivery mcquillanDeliveryFor(std::optional<bool> inStock) {
    if (inStock.has_value() && !*inStock) {
        return core::Delivery{.status = "unavailable", .source = "product"};
    }
    return core::Delivery{
        .status = "available",
        .minDays = 1,
        .maxDays = 3,
        .cost = 6.99,
        .source = "site default"
    };
}

inline core::SearchResult parseMcquillanSearch(std::string_view html, const std::string& checkedAt) {
    auto doc = core::Document::parse(html);
    std::vector<core::Offer> offers;

    auto cards = doc.querySelectorAll(".product-item, .item.product, div.product-card");
    for (const auto& card : cards) {
        auto a = card.querySelector("a.product-item-link, a.product-title, a");
        if (!a) continue;
        auto hrefOpt = a->attr("href");
        if (!hrefOpt) continue;
        std::string href = *hrefOpt;
        std::string title = cleanText(a->text());
        if (title.empty()) continue;

        auto priceEl = card.querySelector(".price, .price-wrapper, span[data-price-type='finalPrice']");
        if (!priceEl) continue;
        auto priceVal = core::parsePrice(priceEl->text());
        if (!priceVal) continue;

        core::Price p;
        p.amount = *priceVal;
        p.taxIncluded = true;
        p.currency = "EUR";

        std::string canonical = core::canonicalUrl(href, MCQUILLAN_BASE);
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
        offer.site = "mcquillantools";
        offer.id = id;
        offer.url = canonical;
        offer.name = title;
        offer.price = p;
        offer.delivery = mcquillanDeliveryFor(std::nullopt);
        offer.details = std::move(details);
        offer.checkedAt = checkedAt;

        offers.push_back(std::move(offer));
    }

    return core::SearchResult{.offers = offers, .total = static_cast<int>(offers.size())};
}

inline core::Offer parseMcquillanProduct(std::string_view html, const std::string& url, const std::string& checkedAt) {
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
        auto h1 = doc.querySelector("h1.page-title span, h1");
        if (h1) title = cleanText(h1->text());
    }

    if (!priceVal) {
        auto pEl = doc.querySelector(".price-final_price .price, .price");
        if (pEl) priceVal = core::parsePrice(pEl->text());
    }

    core::Price p;
    p.amount = priceVal.value_or(0.0);
    p.taxIncluded = true;
    p.currency = "EUR";

    std::string canonical = core::canonicalUrl(url, MCQUILLAN_BASE);
    std::string id = sku.empty() ? canonical : sku;

    std::map<std::string, slimr::Json> details;
    if (!brand.empty()) details["Brand"] = slimr::Json(brand);
    if (!sku.empty()) details["SKU"] = slimr::Json(sku);
    if (!imageUrl.empty()) details["Image"] = slimr::Json(imageUrl);

    core::Offer offer;
    offer.site = "mcquillantools";
    offer.id = id;
    offer.url = canonical;
    offer.name = title.empty() ? "McQuillan Tools Product" : title;
    if (!brand.empty()) offer.brand = brand;
    offer.price = p;
    offer.delivery = mcquillanDeliveryFor(std::nullopt);
    offer.details = std::move(details);
    offer.checkedAt = checkedAt;
    return offer;
}

class McquillanToolsAdapter : public core::SiteAdapter {
public:
    std::string id() const override { return "mcquillantools"; }
    core::SearchResult search(core::SiteContext& ctx, const core::SearchQuery& q) override {
        std::string url = MCQUILLAN_BASE + "catalogsearch/result/?q=" + slimr::url_encode(q.query);
        std::string html = ctx.fetchText(url);
        return parseMcquillanSearch(html, "2026-11-10T00:00:00Z");
    }
    std::vector<core::Offer> offer(core::SiteContext& ctx, const core::OfferRef& ref, const core::OfferOptions&) override {
        std::string url = ref.url.value_or("");
        if (url.empty()) throw std::runtime_error("mcquillantools: offer requires url");
        std::string html = ctx.fetchText(core::canonicalUrl(url, MCQUILLAN_BASE));
        return {parseMcquillanProduct(html, url, "2026-11-10T00:00:00Z")};
    }
};

} // namespace shop::sites
