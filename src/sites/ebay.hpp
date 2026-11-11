#pragma once

#include "site_helpers.hpp"

#include <format>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

namespace shop::sites::ebay_ns {

inline std::string conditionOf(std::string_view label) {
    std::string s = toLower(label);
    static const std::regex refurbRe(R"(refurb|remanufactured)");
    static const std::regex newRe(R"(brand new|^new\b|open box)");
    static const std::regex usedRe(R"(pre-owned|used|parts|not working|acceptable|good|excellent)");
    if (std::regex_search(s, refurbRe)) return "refurbished";
    if (std::regex_search(s, newRe)) return "new";
    if (std::regex_search(s, usedRe)) return "used";
    return "unknown";
}

inline std::optional<double> postageCost(std::string_view line) {
    if (line.empty()) return std::nullopt;
    std::string s(line);
    static const std::regex freeRe(R"(free)", std::regex::icase);
    if (std::regex_search(s, freeRe)) return 0.0;
    static const std::regex costRe(R"(\+?\s*(?:EUR|€|£|US ?\$)\s*([\d.,]+))", std::regex::icase);
    std::smatch m;
    if (std::regex_search(s, m, costRe)) {
        return core::parsePrice(m[1].str());
    }
    return std::nullopt;
}

inline core::Delivery postageDelivery(std::string_view line) {
    auto cost = postageCost(line);
    core::Delivery d;
    d.status = "available";
    if (cost) d.cost = *cost;
    d.source = "listing";
    return d;
}

inline core::SearchResult parseSearch(std::string_view html, const std::string& checkedAt) {
    auto doc = core::Document::parse(html);
    std::vector<core::Offer> offers;
    const std::string BASE = "https://www.ebay.ie/";

    auto cards = doc.querySelectorAll("li.s-card[data-listingid]");
    for (const auto& li : cards) {
        auto idOpt = li.attr("data-listingid");
        if (!idOpt) continue;
        std::string id = *idOpt;
        if (id.length() > 12) continue;

        auto link = li.querySelector("a.s-card__link");
        if (!link) continue;
        auto hrefOpt = link->attr("href");
        if (!hrefOpt) continue;

        auto priceEl = li.querySelector(".s-card__price");
        std::string priceText = cleanText(priceEl ? priceEl->text() : "");

        static const std::regex amountRe(R"(([A-Z]{3})\s*([\d.,]+))");
        std::sregex_iterator it(priceText.begin(), priceText.end(), amountRe);
        std::sregex_iterator end;
        std::vector<std::pair<std::string, std::string>> amounts;
        for (; it != end; ++it) {
            amounts.emplace_back((*it)[1].str(), (*it)[2].str());
        }
        if (amounts.empty()) continue;
        auto amountOpt = core::parsePrice(amounts[0].second);
        if (!amountOpt) continue;
        double amount = *amountOpt;
        std::string currency = amounts[0].first;

        auto titleEl = li.querySelector(".s-card__title");
        std::string title = cleanText(titleEl ? titleEl->text() : "");
        static const std::regex opensNewTabRe(R"(\s*Opens in a new window or tab$)");
        title = std::regex_replace(title, opensNewTabRe, "");

        std::vector<std::string> rows;
        for (const auto& r : li.querySelectorAll(".s-card__attribute-row")) {
            rows.push_back(cleanText(r.text()));
        }

        auto subtitleEl = li.querySelector(".s-card__subtitle");
        std::string subtitle = cleanText(subtitleEl ? subtitleEl->text() : "");
        std::string conditionLabel;
        std::string sellerType;
        size_t pipe = subtitle.find('|');
        if (pipe != std::string::npos) {
            conditionLabel = cleanText(subtitle.substr(0, pipe));
            sellerType = cleanText(subtitle.substr(pipe + 1));
        } else {
            conditionLabel = subtitle;
        }

        std::string bidRow;
        std::string postageRow;
        std::string fromRow;
        std::string sellerRow;
        std::string vatRow;
        bool hasBestOffer = false;

        static const std::regex bidsRe(R"(\bbids?\b)", std::regex::icase);
        static const std::regex postageRe(R"(postage|delivery)", std::regex::icase);
        static const std::regex fromRe(R"(^from )", std::regex::icase);
        static const std::regex sellerPosRe(R"(\d%\s*positive)", std::regex::icase);
        static const std::regex bestOfferRe(R"(or Best Offer)", std::regex::icase);
        static const std::regex vatRe(R"(VAT will apply)", std::regex::icase);

        for (const auto& r : rows) {
            if (bidRow.empty() && std::regex_search(r, bidsRe)) bidRow = r;
            if (postageRow.empty() && std::regex_search(r, postageRe)) postageRow = r;
            if (fromRow.empty() && std::regex_search(r, fromRe)) fromRow = r;
            if (sellerRow.empty() && std::regex_search(r, sellerPosRe)) sellerRow = r;
            if (std::regex_search(r, bestOfferRe)) hasBestOffer = true;
            if (vatRow.empty() && std::regex_search(r, vatRe)) vatRow = r;
        }

        std::string seller = "unknown";
        std::string feedback;
        if (!sellerRow.empty()) {
            static const std::regex sRe(R"(^(.+?)\s+[\d.]+%\s*positive)", std::regex::icase);
            std::smatch m;
            if (std::regex_search(sellerRow, m, sRe)) {
                seller = m[1].str();
            }
            static const std::regex fbRe(R"(([\d.]+%\s*positive(?:\s*\([^)]+\))?))", std::regex::icase);
            if (std::regex_search(sellerRow, m, fbRe)) {
                feedback = m[1].str();
            }
        }

        std::map<std::string, slimr::Json> details;
        details["Format"] = slimr::Json(bidRow.empty() ? "Buy It Now" : "Auction");
        if (hasBestOffer) details["Best Offer"] = slimr::Json(true);
        if (!bidRow.empty()) {
            static const std::regex bNumRe(R"(^(\d+) bids?)", std::regex::icase);
            std::smatch m;
            if (std::regex_search(bidRow, m, bNumRe)) {
                details["Bids"] = slimr::Json(static_cast<int64_t>(std::stoll(m[1].str())));
            }
            static const std::regex leftRe(R"(Time left\s*(.+)$)", std::regex::icase);
            if (std::regex_search(bidRow, m, leftRe)) {
                details["Time left"] = slimr::Json(cleanText(m[1].str()));
            }
        }
        if (!fromRow.empty()) {
            static const std::regex fStripRe(R"(^from )", std::regex::icase);
            details["Item location"] = slimr::Json(cleanText(std::regex_replace(fromRow, fStripRe, "")));
        }
        if (!feedback.empty()) details["Seller feedback"] = slimr::Json(feedback);
        if (!sellerType.empty()) details["Seller type"] = slimr::Json(sellerType);
        if (!conditionLabel.empty()) details["Condition label"] = slimr::Json(conditionLabel);
        if (amounts.size() > 1) details["Price range"] = slimr::Json(priceText);
        if (!vatRow.empty()) {
            static const std::regex vatStripRe(R"(^\+\s*)");
            details["Import VAT"] = slimr::Json(cleanText(std::regex_replace(vatRow, vatStripRe, "")));
        }

        core::Offer offer;
        offer.site = "ebay";
        offer.id = id;
        offer.url = BASE + "itm/" + id;
        offer.name = title;
        offer.seller = seller;
        offer.condition = conditionOf(conditionLabel);
        offer.price = core::Price{.amount = amount, .currency = currency, .taxIncluded = true};
        offer.delivery = postageDelivery(postageRow);
        offer.details = std::move(details);
        offer.checkedAt = checkedAt;

        offers.push_back(std::move(offer));
    }

    std::optional<int> total;
    auto headingEl = doc.querySelector(".srp-controls__count-heading");
    if (headingEl) {
        std::string heading = cleanText(headingEl->text());
        static const std::regex countRe(R"(^([\d,]+)\+?\s+results?)", std::regex::icase);
        std::smatch m;
        if (std::regex_search(heading, m, countRe)) {
            auto parsedTotal = core::parsePrice(m[1].str());
            if (parsedTotal) total = static_cast<int>(*parsedTotal);
        }
    }

    return core::SearchResult{.offers = std::move(offers), .total = total};
}

inline core::Offer parseProduct(std::string_view html, std::string_view id, const std::string& checkedAt) {
    auto doc = core::Document::parse(html);
    const std::string BASE = "https://www.ebay.ie/";

    auto t = [&](std::string_view sel) -> std::string {
        auto node = doc.querySelector(sel);
        return cleanText(node ? node->text() : "");
    };

    auto blocks = core::jsonLdBlocks(html);
    auto product = core::findLd(blocks, "Product");

    std::optional<double> amount;
    std::string currency;

    if (product && product->is_object() && product->contains("offers")) {
        const auto& ld = (*product)["offers"];
        if (ld.is_object()) {
            if (ld.contains("price")) {
                if (ld["price"].is_string()) amount = core::parsePrice(ld["price"].get<std::string>());
                else if (ld["price"].is_number()) amount = ld["price"].as<double>();
            }
            if (ld.contains("priceCurrency") && ld["priceCurrency"].is_string()) {
                currency = cleanText(ld["priceCurrency"].get<std::string>());
            }
        }
    }

    std::string listed = t(".x-price-primary");
    if (!amount) {
        std::string approxText = t(".x-price-approx");
        static const std::regex approxRe(R"(([A-Z]{3})\s*([\d.,]+))");
        std::smatch m;
        if (std::regex_search(approxText, m, approxRe)) {
            amount = core::parsePrice(m[2].str());
            currency = m[1].str();
        } else {
            amount = core::parsePrice(listed);
            if (listed.find("£") != std::string::npos) currency = "GBP";
            else if (listed.find("€") != std::string::npos || listed.find("EUR") != std::string::npos) currency = "EUR";
            else if (listed.find("US $") != std::string::npos || listed.find("$") != std::string::npos) currency = "USD";
        }
    }

    if (!amount || currency.empty()) {
        throw std::runtime_error("ebay: no price on the item page (layout changed?)");
    }

    std::string conditionText = t(".x-item-condition-text");
    static const std::regex moreInfoRe(R"(More information.*$)", std::regex::icase);
    conditionText = std::regex_replace(conditionText, moreInfoRe, "");
    conditionText = cleanText(conditionText);

    std::string conditionLd;
    if (product && product->is_object() && product->contains("offers")) {
        const auto& ld = (*product)["offers"];
        if (ld.is_object() && ld.contains("itemCondition") && ld["itemCondition"].is_string()) {
            conditionLd = ld["itemCondition"].get<std::string>();
        }
    }

    std::string condition;
    if (conditionLd.find("NewCondition") != std::string::npos) condition = "new";
    else if (conditionLd.find("RefurbishedCondition") != std::string::npos) condition = "refurbished";
    else if (conditionLd.find("UsedCondition") != std::string::npos || conditionLd.find("DamagedCondition") != std::string::npos) condition = "used";
    else condition = conditionOf(conditionText);

    std::string sellerText = t(".x-sellercard-atf");
    std::string seller = "unknown";
    std::string sellerCount;
    std::string sellerPct;
    static const std::regex sellerRe(R"(^(\S+?)\s*\(([\d.,KM]+)\))");
    std::smatch sm;
    if (std::regex_search(sellerText, sm, sellerRe)) {
        seller = sm[1].str();
        sellerCount = sm[2].str();
    }
    static const std::regex pctRe(R"(([\d.]+%)\s*positive)", std::regex::icase);
    if (std::regex_search(sellerText, sm, pctRe)) {
        sellerPct = sm[1].str();
    }

    std::string ship = t(".ux-labels-values--shipping");
    std::string returns = t(".ux-labels-values--returns");
    static const std::regex retPrefixRe(R"(^Returns:\s*)", std::regex::icase);
    returns = std::regex_replace(returns, retPrefixRe, "");

    std::string located;
    static const std::regex locRe(R"(Located in:\s*(.+)$)", std::regex::icase);
    if (std::regex_search(ship, sm, locRe)) {
        located = cleanText(sm[1].str());
    }

    std::string bidCountText = t(".x-bid-count");
    if (bidCountText.empty()) bidCountText = t(".x-bid-price");
    std::optional<int> bids;
    static const std::regex bRe(R"((\d+) bids?)", std::regex::icase);
    if (std::regex_search(bidCountText, sm, bRe)) {
        bids = std::stoi(sm[1].str());
    }

    std::string timerText = t(".x-timer-module");
    if (timerText.empty()) timerText = t(".x-bid-price");
    std::string left;
    static const std::regex leftRe(R"(Ends in\s*(\d[^()]*?)(?:\s*\(|$))", std::regex::icase);
    if (std::regex_search(timerText, sm, leftRe)) {
        left = cleanText(sm[1].str());
    }

    std::map<std::string, slimr::Json> details;
    details["Format"] = slimr::Json(bids.has_value() ? "Auction" : "Buy It Now");
    if (bids.has_value()) details["Bids"] = slimr::Json(static_cast<int64_t>(*bids));
    if (!left.empty()) details["Time left"] = slimr::Json(left);
    if (!located.empty()) details["Item location"] = slimr::Json(located);
    if (!returns.empty()) {
        static const std::regex seeDetRe(R"(\s*See details.*$)", std::regex::icase);
        details["Returns"] = slimr::Json(cleanText(std::regex_replace(returns, seeDetRe, "")));
    }
    if (!conditionText.empty()) details["Condition label"] = slimr::Json(conditionText);
    if (!sellerText.empty()) {
        static const std::regex regBusRe(R"(Registered as a business)", std::regex::icase);
        details["Seller type"] = slimr::Json(std::regex_search(sellerText, regBusRe) ? "Business" : "Private");
        if (!sellerPct.empty()) {
            std::string fb = sellerPct + " positive";
            if (!sellerCount.empty()) fb += " (" + sellerCount + ")";
            details["Seller feedback"] = slimr::Json(fb);
        }
    }
    std::string qty = t(".x-quantity__availability");
    if (!qty.empty()) details["Availability"] = slimr::Json(qty);
    static const std::regex bestOfferListedRe(R"(or Best Offer)", std::regex::icase);
    if (std::regex_search(listed, bestOfferListedRe)) details["Best Offer"] = slimr::Json(true);

    std::optional<std::string> brand;
    if (product && product->is_object() && product->contains("brand")) {
        const auto& b = (*product)["brand"];
        if (b.is_object() && b.contains("name") && b["name"].is_string()) {
            std::string bname = cleanText(b["name"].get<std::string>());
            static const std::regex unbrandedRe(R"(^unbranded$|^does not apply$)", std::regex::icase);
            if (!bname.empty() && !std::regex_match(bname, unbrandedRe)) {
                brand = bname;
            }
        }
    }

    std::optional<std::string> gtin;
    if (product && product->is_object() && product->contains("gtin13")) {
        if ((*product)["gtin13"].is_string()) {
            std::string g = cleanText((*product)["gtin13"].get<std::string>());
            static const std::regex gtinRe(R"(^\d{8,14}$)");
            if (std::regex_match(g, gtinRe)) gtin = g;
        }
    }

    std::optional<double> cost;
    if (product && product->is_object() && product->contains("offers")) {
        const auto& ld = (*product)["offers"];
        if (ld.is_object() && ld.contains("shippingDetails") && ld["shippingDetails"].is_array()) {
            const auto& sdArr = ld["shippingDetails"].get<slimr::Json::array_t>();
            if (!sdArr.empty() && sdArr[0].is_object() && sdArr[0].contains("shippingRate")) {
                const auto& sr = sdArr[0]["shippingRate"];
                if (sr.is_object() && sr.contains("value") && sr["value"].is_string()) {
                    cost = core::parsePrice(sr["value"].get<std::string>());
                }
            }
        }
    }
    if (!cost) cost = postageCost(ship);

    std::optional<bool> inStock;
    if (product && product->is_object() && product->contains("offers")) {
        const auto& ld = (*product)["offers"];
        if (ld.is_object() && ld.contains("availability") && ld["availability"].is_string()) {
            std::string avail = ld["availability"].get<std::string>();
            static const std::regex outRe(R"(OutOfStock|SoldOut)");
            static const std::regex inRe(R"(InStock)");
            if (std::regex_search(avail, outRe)) inStock = false;
            else if (std::regex_search(avail, inRe)) inStock = true;
        }
    }

    std::string name = (product && product->is_object() && product->contains("name") && (*product)["name"].is_string())
                       ? cleanText((*product)["name"].get<std::string>())
                       : t("h1");

    core::Offer offer;
    offer.site = "ebay";
    offer.id = std::string(id);
    offer.url = BASE + "itm/" + std::string(id);
    offer.name = name;
    if (brand) offer.brand = *brand;
    if (gtin) offer.gtin = *gtin;
    offer.seller = seller;
    offer.condition = condition;
    offer.price = core::Price{.amount = *amount, .currency = currency, .taxIncluded = true};
    if (inStock.has_value()) offer.inStock = *inStock;
    if (cost.has_value()) {
        offer.delivery = core::Delivery{.status = "available", .cost = *cost, .source = "listing"};
    } else {
        offer.delivery = postageDelivery(ship);
    }
    offer.details = std::move(details);
    offer.checkedAt = checkedAt;

    return offer;
}

class Ebay : public core::SiteAdapter {
public:
    std::string id() const override { return "ebay"; }
    bool deliveryOnly() const override { return true; }
    std::string matchHint() const override {
        return "eBay matches every word in the title; use (a,b) for either word and -word to exclude. Try fewer, plainer words.";
    }

    core::SearchResult search(core::SiteContext& ctx, const core::SearchQuery& q) override {
        std::string params = "_nkw=" + urlEncode(q.query, true) + "&_sop=12";
        std::vector<std::string> nativeFilters;
        if (q.filters.minPrice) {
            params += "&_udlo=" + std::to_string(*q.filters.minPrice);
            nativeFilters.push_back("minPrice");
        }
        if (q.filters.maxPrice) {
            params += "&_udhi=" + std::to_string(*q.filters.maxPrice);
            nativeFilters.push_back("maxPrice");
        }
        std::string html = ctx.fetchText("https://www.ebay.ie/sch/i.html?" + params);
        auto res = parseSearch(html, "2026-11-10T00:00:00Z");
        res.nativeFilters = std::move(nativeFilters);
        return res;
    }

    std::vector<core::Offer> offer(core::SiteContext& ctx, const core::OfferRef& ref, const core::OfferOptions& options) override {
        (void)options;
        std::string id = ref.id.value_or("");
        if (id.empty() && ref.url) {
            static const std::regex itmRe(R"(/itm/(?:[^/?]+/)?(\d{9,12}))");
            std::smatch m;
            if (std::regex_search(*ref.url, m, itmRe)) {
                id = m[1].str();
            }
        }
        if (id.empty()) {
            throw std::runtime_error("ebay: pass an item id (12 digits) or an /itm/ URL");
        }
        std::string html = ctx.fetchText("https://www.ebay.ie/itm/" + id);
        return {parseProduct(html, id, "2026-11-10T00:00:00Z")};
    }
};

} // namespace shop::sites::ebay_ns
namespace shop::sites {
using Ebay = ebay_ns::Ebay;
using EbayAdapter = ebay_ns::Ebay;
} // namespace shop::sites
