#pragma once

#include "site_helpers.hpp"

#include <format>
#include <regex>
#include <string>
#include <vector>

namespace shop::sites {

inline const std::string DONEDEAL_BASE = "https://www.donedeal.ie/";

inline core::Delivery donedealDeliveryFor() {
    return core::Delivery{
        .status = "unknown",
        .source = "site default"
    };
}

inline core::SearchResult parseDonedealSearch(std::string_view html, const std::string& checkedAt) {
    auto doc = core::Document::parse(html);
    std::vector<core::Offer> offers;

    auto cards = doc.querySelectorAll("article, div.card, a[href*='/ads/']");
    for (const auto& card : cards) {
        auto a = (card.tagName() == "a") ? card : *card.querySelector("a");
        std::string href;
        if (auto h = a.attr("href")) href = *h;
        if (href.empty()) continue;

        auto titleEl = card.querySelector("h2, h3, .card__title, span[data-testid='ad-title']");
        std::string title = titleEl ? cleanText(titleEl->text()) : cleanText(a.text());
        if (title.empty()) continue;

        auto priceEl = card.querySelector(".card__price, span[data-testid='ad-price'], .price");
        if (!priceEl) continue;
        auto priceVal = core::parsePrice(priceEl->text());
        if (!priceVal) continue;

        core::Price p;
        p.amount = *priceVal;
        p.taxIncluded = true;
        p.currency = "EUR";

        std::string canonical = core::canonicalUrl(href, DONEDEAL_BASE);
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
        offer.site = "donedeal";
        offer.id = id;
        offer.url = canonical;
        offer.name = title;
        offer.price = p;
        offer.delivery = donedealDeliveryFor();
        offer.details = std::move(details);
        offer.checkedAt = checkedAt;

        offers.push_back(std::move(offer));
    }

    return core::SearchResult{.offers = offers, .total = static_cast<int>(offers.size())};
}

inline core::Offer parseDonedealProduct(std::string_view html, const std::string& url, const std::string& checkedAt) {
    auto doc = core::Document::parse(html);
    auto blocks = core::jsonLdBlocks(html);
    auto ld = core::findLd(blocks, "Product");

    std::string title;
    std::optional<double> priceVal;
    std::string imageUrl;

    if (ld && ld->is_object()) {
        if ((*ld).contains("name") && (*ld)["name"].is_string()) title = cleanText((*ld)["name"].get<std::string>());
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
        auto pEl = doc.querySelector(".price, span[data-testid='ad-price']");
        if (pEl) priceVal = core::parsePrice(pEl->text());
    }

    core::Price p;
    p.amount = priceVal.value_or(0.0);
    p.taxIncluded = true;
    p.currency = "EUR";

    std::string canonical = core::canonicalUrl(url, DONEDEAL_BASE);

    std::map<std::string, slimr::Json> details;
    if (!imageUrl.empty()) details["Image"] = slimr::Json(imageUrl);

    core::Offer offer;
    offer.site = "donedeal";
    offer.id = canonical;
    offer.url = canonical;
    offer.name = title.empty() ? "DoneDeal Ad" : title;
    offer.price = p;
    offer.delivery = donedealDeliveryFor();
    offer.details = std::move(details);
    offer.checkedAt = checkedAt;
    return offer;
}

class DonedealAdapter : public core::SiteAdapter {
public:
    std::string id() const override { return "donedeal"; }
    core::SearchResult search(core::SiteContext& ctx, const core::SearchQuery& q) override {
        std::string url = DONEDEAL_BASE + "all?words=" + slimr::url_encode(q.query);
        std::string html = ctx.fetchText(url);
        return parseDonedealSearch(html, "2026-11-10T00:00:00Z");
    }
    std::vector<core::Offer> offer(core::SiteContext& ctx, const core::OfferRef& ref, const core::OfferOptions&) override {
        std::string url = ref.url.value_or("");
        if (url.empty()) throw std::runtime_error("donedeal: offer requires url");
        std::string html = ctx.fetchText(core::canonicalUrl(url, DONEDEAL_BASE));
        return {parseDonedealProduct(html, url, "2026-11-10T00:00:00Z")};
    }
};

} // namespace shop::sites
