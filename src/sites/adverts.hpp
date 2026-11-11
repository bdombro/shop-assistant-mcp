#pragma once

#include "site_utils.hpp"

namespace shop::sites {

inline std::string advertsConditionOf(std::string_view s) {
    static const std::regex asNewRe(R"(\b(?:as|like|almost|nearly)\s+new\b)", std::regex::icase);
    static const std::regex newRe(
        R"(\bbrand[- ]new\b|\bnew in (?:the )?(?:box|packaging)\b|\bnib\b|\bsealed\b|\bunopened\b|\bnever (?:been )?used\b|\bunused\b)",
        std::regex::icase);
    std::string str(s);
    if (std::regex_search(str, asNewRe)) return "used";
    if (std::regex_search(str, newRe)) return "new";
    return "used";
}

inline std::optional<double> advertsPriceOf(std::string_view raw) {
    std::string s = cleanText(raw);
    static const std::regex freeRe(R"(^free$)", std::regex::icase);
    if (std::regex_match(s, freeRe)) return 0.0;
    static const std::regex euroRe(R"(^€\s*[\d.,]+$)");
    if (std::regex_match(s, euroRe)) return parsePrice(s);
    return std::nullopt;
}

inline SearchResult parseAdvertsSearch(std::string_view html, std::string_view checkedAt) {
    auto document = Document::parse(html);
    std::vector<Offer> offers;
    const std::string BASE = "https://www.adverts.ie/";

    auto rows = document.querySelectorAll("div.search_result");
    for (const auto& row : rows) {
        auto link = row.querySelector("h4 a");
        std::string href = link ? link->attr("href").value_or("") : "";
        auto btn = row.querySelector("button.quick-peek-btn");
        std::string id = btn ? btn->attr("data-ad-id").value_or("") : "";
        if (id.empty() && !href.empty()) {
            static const std::regex idRe(R"(/(\d+)$)");
            std::smatch m;
            if (std::regex_search(href, m, idRe)) {
                id = m[1].str();
            }
        }
        auto dd = row.querySelector(".holder.header dd");
        auto amount = advertsPriceOf(cleanText(dd));
        if (id.empty() || href.empty() || !amount.has_value() || *amount <= 0.0) continue;

        std::string name = cleanText(link);
        std::string snippet = cleanText(row.querySelector(".item_description"));

        std::string location;
        for (const auto& li : row.querySelectorAll("li")) {
            std::string cls = li.attr("class").value_or("");
            if (cls.find("first") == std::string::npos && cls.find("search-icons") == std::string::npos) {
                location = cleanText(li.text());
                static const std::regex spComma(R"(\s+,)");
                location = std::regex_replace(location, spComma, ",");
                break;
            }
        }

        std::string age;
        std::optional<int64_t> comments;
        static const std::regex agoRe(R"(\bago\b)", std::regex::icase);
        static const std::regex commentsRe(R"(^(\d+)\s+comments?)", std::regex::icase);
        for (const auto& li : row.querySelectorAll("ul.date-entered li")) {
            std::string item = cleanText(li.text());
            if (std::regex_search(item, agoRe)) {
                age = item;
            }
            std::smatch cm;
            if (std::regex_search(item, cm, commentsRe)) {
                comments = std::stoll(cm[1].str());
            }
        }

        std::string category = cleanText(row.querySelector(".category_name_span"));
        bool topSeller = row.querySelector(".top_seller_badge").has_value();

        std::map<std::string, slimr::Json> details;
        if (!location.empty()) details["Location"] = slimr::Json(location);
        if (!age.empty()) details["Ad age"] = slimr::Json(age);
        if (!category.empty()) details["Category"] = slimr::Json(category);
        if (topSeller) details["Top seller"] = slimr::Json(true);
        if (comments.has_value()) details["Comments"] = slimr::Json(*comments);
        if (!snippet.empty()) details["Description"] = slimr::Json(snippet);

        std::string seller = cleanText(row.querySelector("li.first a[href^='/member/']"));
        if (seller.empty()) seller = "unknown";

        Offer offer;
        offer.site = "adverts";
        offer.id = id;
        offer.url = canonicalUrl(href, BASE);
        offer.name = name;
        offer.seller = seller;
        offer.condition = advertsConditionOf(name + " " + snippet);
        offer.price = Price{ *amount, "EUR", std::nullopt, true };
        offer.delivery = Delivery{ "unknown", std::nullopt, std::nullopt, std::nullopt, "listing" };
        offer.details = std::move(details);
        offer.checkedAt = std::string(checkedAt);
        offers.push_back(std::move(offer));
    }

    SearchResult res;
    res.offers = std::move(offers);
    auto srCount = document.querySelector("#sr-count");
    if (srCount) {
        static const std::regex countRe(R"(([\d,]+)\s+results?)", std::regex::icase);
        std::string txt = cleanText(srCount);
        std::smatch m;
        if (std::regex_search(txt, m, countRe)) {
            auto totalVal = parsePrice(m[1].str());
            if (totalVal.has_value()) res.total = static_cast<int>(*totalVal);
        }
    }
    return res;
}

inline Offer parseAdvertsProduct(std::string_view html, std::string_view id, std::string_view checkedAt) {
    auto document = Document::parse(html);
    const std::string BASE = "https://www.adverts.ie/";

    auto productLd = findLd(document.jsonLd(), "Product");
    slimr::Json ldOffer;
    if (productLd.has_value() && productLd->is_object() && productLd->contains("offers")) {
        ldOffer = (*productLd)["offers"];
    }

    std::optional<double> amount;
    if (ldOffer.is_object() && ldOffer.contains("price")) {
        amount = parsePrice(ldOffer["price"]);
    }
    if (!amount.has_value()) {
        amount = advertsPriceOf(cleanText(document.querySelector(".ad_view_price .price")));
    }
    if (!amount.has_value()) {
        throw std::runtime_error("adverts: no price on the ad page (no price, swap or wanted ad?)");
    }

    std::string name;
    if (productLd.has_value() && productLd->is_object() && productLd->contains("name")) {
        name = cleanText(jsonString((*productLd)["name"]));
    }
    if (name.empty()) {
        name = cleanText(document.querySelector("h1"));
    }

    std::string description;
    if (productLd.has_value() && productLd->is_object() && productLd->contains("description")) {
        description = cleanText(jsonString((*productLd)["description"]));
    }

    std::string condition;
    std::string ldCondition = ldOffer.is_object() && ldOffer.contains("itemCondition")
        ? jsonString(ldOffer["itemCondition"]) : "";
    if (ldCondition.find("NewCondition") != std::string::npos) {
        condition = "new";
    } else if (ldCondition.find("Refurbished") != std::string::npos) {
        condition = "refurbished";
    } else {
        condition = advertsConditionOf(name + " " + description);
    }

    std::string location = cleanText(document.querySelector(".smi-location-links"));
    static const std::regex spComma(R"(\s+,)");
    location = std::regex_replace(location, spComma, ",");

    std::map<std::string, std::string> rows;
    for (const auto& dt : document.querySelectorAll("dl.ad_view_properties_list dt")) {
        auto dd = nextElementSibling(dt);
        std::string label = cleanText(dt.text());
        if (!label.empty() && label.back() == ':') label.pop_back();
        if (!label.empty() && dd && dd->tagName() == "dd") {
            rows[label] = cleanText(dd->text());
        }
    }

    std::map<std::string, slimr::Json> details;
    if (!location.empty()) details["Location"] = slimr::Json(location);

    auto ageIt = rows.find("Entered/Renewed");
    if (ageIt != rows.end()) {
        details["Ad age (entered or renewed)"] = slimr::Json(ageIt->second);
    }

    std::vector<std::string> feedback;
    for (const auto& e : document.querySelectorAll(".feedback-trust__count")) {
        feedback.push_back(cleanText(e.text()));
    }
    if (feedback.size() >= 2) {
        details["Seller feedback"] = slimr::Json(feedback[0] + " positive, " + feedback[1] + " negative");
    }

    static const std::regex skipRows(R"(^(Seller|Feedback|Location|Entered/Renewed)$)");
    for (const auto& [label, value] : rows) {
        if (value.empty() || std::regex_match(label, skipRows)) continue;
        if (label == "Ad Views") {
            auto views = parsePrice(value);
            if (views.has_value()) {
                details["Ad views"] = slimr::Json(static_cast<int64_t>(*views));
            } else {
                details["Ad views"] = slimr::Json(value);
            }
        } else {
            details[label] = slimr::Json(value);
        }
    }

    if (!description.empty()) {
        if (description.size() > 600) {
            details["Description"] = slimr::Json(description.substr(0, 600) + "\xE2\x80\xA6");
        } else {
            details["Description"] = slimr::Json(description);
        }
    }

    std::string seller = cleanText(document.querySelector("a.sellername"));
    if (seller.empty()) seller = "unknown";

    std::string url;
    if (ldOffer.is_object() && ldOffer.contains("url")) {
        url = cleanText(jsonString(ldOffer["url"]));
    }
    if (url.empty()) {
        url = canonicalUrl("/" + std::string(id), BASE);
    }

    Offer offer;
    offer.site = "adverts";
    offer.id = std::string(id);
    offer.url = url;
    offer.name = name;
    offer.seller = seller;
    offer.condition = condition;
    offer.price = Price{ *amount, "EUR", std::nullopt, true };
    offer.delivery = Delivery{ "unknown", std::nullopt, std::nullopt, std::nullopt, "listing" };
    offer.details = std::move(details);
    offer.checkedAt = std::string(checkedAt);
    return offer;
}

class AdvertsAdapter : public SiteAdapter {
public:
    std::string id() const override { return "adverts"; }

    std::string matchHint() const override {
        return "Adverts.ie matches ads containing the words in the title or description; use fewer, plainer words (a model name, not a sentence).";
    }

    SearchResult search(SiteContext& ctx, const SearchQuery& q) override {
        const std::string BASE = "https://www.adverts.ie/";
        std::string price;
        if (q.filters.minPrice.has_value() || q.filters.maxPrice.has_value()) {
            price = "price_" + std::to_string(static_cast<int>(std::floor(q.filters.minPrice.value_or(0.0)))) + "-";
            if (q.filters.maxPrice.has_value()) {
                price += std::to_string(static_cast<int>(std::ceil(*q.filters.maxPrice)));
            } else {
                price += "*";
            }
            price += "/";
        }
        std::string url = BASE + "for-sale/q_" + urlEncode(q.query) + "/" + price + "list_view/";
        std::string html = ctx.fetchText(url);
        std::string checkedAt = "2026-10-15T00:00:00Z"; // Default or current time
        auto res = parseAdvertsSearch(html, checkedAt);
        if (!price.empty()) {
            res.nativeFilters.push_back("minPrice");
            res.nativeFilters.push_back("maxPrice");
        }
        return res;
    }

    std::vector<Offer> offer(SiteContext& ctx, const OfferRef& ref, const OfferOptions& options) override {
        (void)options;
        const std::string BASE = "https://www.adverts.ie/";
        std::string id = ref.id.value_or("");
        if (id.empty() && ref.url.has_value()) {
            static const std::regex idRe(R"(/(\d{5,10})(?:[/?#]|$))");
            std::smatch m;
            if (std::regex_search(*ref.url, m, idRe)) {
                id = m[1].str();
            }
        }
        if (id.empty()) {
            throw std::runtime_error("adverts: pass an ad id or an ad URL");
        }
        std::string html = ctx.fetchText(BASE + id);
        return { parseAdvertsProduct(html, id, "2026-10-15T00:00:00Z") };
    }
};

} // namespace shop::sites
