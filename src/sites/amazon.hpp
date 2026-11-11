#pragma once

#include "site_utils.hpp"

namespace shop::sites {

inline std::optional<std::string> amazonAsinOf(std::string_view url) {
    static const std::regex asinRe(R"(/(?:dp|product)/([A-Z0-9]{10})(?:[/?]|$))");
    std::string s(url);
    std::smatch m;
    if (std::regex_search(s, m, asinRe)) {
        return m[1].str();
    }
    return std::nullopt;
}

inline std::string amazonProductUrl(std::string_view host, std::string_view asin) {
    return canonicalUrl("/dp/" + std::string(asin), "https://" + std::string(host) + "/");
}

inline std::string amazonCurrencyOf(std::string_view shown, std::string_view host) {
    if (shown.find("€") != std::string_view::npos) return "EUR";
    if (shown.find("£") != std::string_view::npos) return "GBP";
    if (shown.find("$") != std::string_view::npos) return "USD";
    if (host.ends_with(".co.uk")) return "GBP";
    if (host.ends_with(".com")) return "USD";
    return "EUR";
}

inline std::string amazonConditionOf(std::string_view label) {
    static const std::regex refRe(R"(\b(renewed|refurbished)\b)", std::regex::icase);
    static const std::regex usedRe(R"(\b(used|pre-owned|open box)\b)", std::regex::icase);
    std::string s(label);
    if (std::regex_search(s, refRe)) return "refurbished";
    if (std::regex_search(s, usedRe)) return "used";
    return "new";
}

inline std::optional<int> amazonReviewCount(std::string_view label) {
    static const std::regex r1(R"(\(([\d.,]+)\s*([KkMm])?\))");
    static const std::regex r2(R"(([\d.,]+)\s*([KkMm])?\s+(?:ratings|global ratings))");
    std::string s(label);
    std::smatch m;
    if (std::regex_search(s, m, r1) || std::regex_search(s, m, r2)) {
        std::string numStr = m[1].str();
        numStr.erase(std::remove(numStr.begin(), numStr.end(), ','), numStr.end());
        try {
            double n = std::stod(numStr);
            if (m[2].matched) {
                char unit = static_cast<char>(std::tolower(static_cast<unsigned char>(m[2].str()[0])));
                if (unit == 'k') n *= 1000.0;
                else if (unit == 'm') n *= 1000000.0;
            }
            return static_cast<int>(std::round(n));
        } catch (...) {}
    }
    return std::nullopt;
}

inline Delivery amazonDeliveryOf(std::string_view line, std::string_view source, std::optional<bool> stock = std::nullopt) {
    if (stock.has_value() && !*stock) {
        return Delivery{ "unavailable", std::nullopt, std::nullopt, std::nullopt, std::string(source) };
    }
    static const std::regex delRe(R"(delivery)", std::regex::icase);
    std::string s(line);
    if (s.empty() || !std::regex_search(s, delRe)) {
        return Delivery{ "unknown", std::nullopt, std::nullopt, std::nullopt, "site default" };
    }
    std::optional<double> cost;
    static const std::regex freeRe(R"(free delivery)", std::regex::icase);
    if (std::regex_search(s, freeRe)) {
        cost = 0.0;
    } else {
        static const std::regex costRe(R"(([€£$]\s?[\d.,]+)\s+delivery)", std::regex::icase);
        std::smatch m;
        if (std::regex_search(s, m, costRe)) {
            cost = parsePrice(m[1].str());
        }
    }
    return Delivery{ "available", std::nullopt, std::nullopt, cost, std::string(source) };
}

inline SearchResult parseAmazonSearch(std::string_view html, std::string_view host, std::string_view checkedAt) {
    auto document = Document::parse(html);
    std::vector<Offer> offers;
    std::vector<std::string> seen;

    auto rows = document.querySelectorAll("[data-component-type=\"s-search-result\"]");
    for (const auto& row : rows) {
        std::string asin = row.attr("data-asin").value_or("");
        if (asin.empty() || std::find(seen.begin(), seen.end(), asin) != seen.end()) continue;
        if (row.querySelector(".puis-sponsored-label-text") || row.querySelector(".s-sponsored-label-text")) continue;

        auto priceNode = row.querySelector(".a-price:not(.a-text-price) .a-offscreen");
        std::string priceText = cleanText(priceNode);
        auto amount = parsePrice(priceText);
        if (!amount.has_value()) continue;

        auto heads = row.querySelectorAll("h2");
        std::string title;
        for (const auto& h : heads) {
            auto aria = h.attr("aria-label");
            if (aria.has_value() && !aria->empty()) {
                title = cleanText(*aria);
                break;
            }
        }
        if (title.empty() && !heads.empty()) {
            title = cleanText(heads.back().text());
        }
        if (title.empty()) continue;

        seen.push_back(asin);
        std::optional<std::string> brand;
        if (heads.size() > 1) {
            brand = cleanText(heads.front().text());
        }

        auto wasNode = row.querySelector(".a-price.a-text-price[data-a-strike=\"true\"] .a-offscreen");
        auto was = parsePrice(cleanText(wasNode));

        auto iconAlt = row.querySelector(".a-icon-alt");
        double ratingValue = 0.0;
        bool hasRating = false;
        if (iconAlt) {
            auto r = parsePrice(cleanText(iconAlt));
            if (r.has_value()) {
                ratingValue = *r;
                hasRating = true;
            }
        }

        auto count = amazonReviewCount(cleanText(row.querySelector("[data-cy=\"reviews-block\"]")));

        std::string otherOffers = cleanText(row.querySelector("[data-cy=\"secondary-offer-recipe\"]"));
        static const std::regex moreBuying(R"(^More buying choices\s*)");
        otherOffers = std::regex_replace(otherOffers, moreBuying, "");

        std::string deliveryLine = cleanText(row.querySelector("[data-cy=\"delivery-recipe\"]"));
        auto delivery = amazonDeliveryOf(deliveryLine, "listing");

        std::map<std::string, slimr::Json> details;
        if (!otherOffers.empty()) {
            details["Other offers"] = slimr::Json(otherOffers);
        }

        Offer offer;
        offer.site = "amazon";
        offer.id = asin;
        offer.url = amazonProductUrl(host, asin);
        offer.name = title;
        if (brand.has_value() && !brand->empty()) offer.brand = *brand;
        offer.seller = "unknown";
        offer.condition = amazonConditionOf(title);
        offer.price = Price{ *amount, amazonCurrencyOf(priceText, host), (was.has_value() && *was > *amount) ? was : std::nullopt, true };
        if (hasRating) {
            offer.rating = Rating{ ratingValue, 5.0, count, std::nullopt };
        }
        offer.delivery = delivery;
        offer.details = std::move(details);
        offer.checkedAt = std::string(checkedAt);
        offers.push_back(std::move(offer));
    }
    return SearchResult{ std::move(offers), std::nullopt, {} };
}

struct TwisterResult {
    std::vector<std::string> variants;
    std::optional<std::string> parent;
    std::string varies;
};

inline TwisterResult amazonTwisterOf(const Document& document) {
    auto asinEl = document.querySelector("#ASIN");
    std::string own = asinEl ? asinEl->attr("value").value_or("") : "";

    std::string state;
    for (const auto& s : document.querySelectorAll("#twister_feature_div script")) {
        std::string txt = s.text();
        if (txt.find("sortedDimValuesForAllDims") != std::string::npos) {
            state = std::move(txt);
            break;
        }
    }

    std::optional<std::string> parent;
    std::string rootHtml = document.documentElement().outerHtml();
    static const std::regex parentRe(R"re("parentAsin":"(\w+)")re");
    std::smatch pm;
    if (std::regex_search(rootHtml, pm, parentRe)) {
        parent = pm[1].str();
    }

    if (state.empty()) {
        std::vector<std::string> vars;
        if (!own.empty()) vars.push_back(own);
        return TwisterResult{ std::move(vars), parent, "" };
    }

    auto parsed = slimr::parse_json<slimr::Json>(state);
    if (!parsed || !parsed->is_object() || !parsed->contains("sortedDimValuesForAllDims")) {
        std::vector<std::string> vars;
        if (!own.empty()) vars.push_back(own);
        return TwisterResult{ std::move(vars), parent, "" };
    }

    auto& dims = (*parsed)["sortedDimValuesForAllDims"];
    std::vector<std::string> variants;
    std::vector<std::string> selected;

    if (dims.is_object()) {
        for (const auto& [_, dimList] : dims.get<slimr::Json::object_t>()) {
            if (!dimList.is_array()) continue;
            for (const auto& v : dimList.get<slimr::Json::array_t>()) {
                if (!v.is_object()) continue;
                if (v.contains("defaultAsin") && v["defaultAsin"].is_string()) {
                    std::string a = v["defaultAsin"].get<std::string>();
                    if (std::find(variants.begin(), variants.end(), a) == variants.end()) {
                        variants.push_back(a);
                    }
                }
                if (v.contains("dimensionValueState") && v["dimensionValueState"].is_string() &&
                    v["dimensionValueState"].get<std::string>() == "SELECTED" &&
                    v.contains("dimensionValueDisplayText") && v["dimensionValueDisplayText"].is_string()) {
                    selected.push_back(v["dimensionValueDisplayText"].get<std::string>());
                }
            }
        }
    }

    if (!own.empty() && std::find(variants.begin(), variants.end(), own) == variants.end()) {
        variants.insert(variants.begin(), own);
    }

    std::string varies;
    for (size_t i = 0; i < selected.size(); ++i) {
        if (i > 0) varies += ", ";
        varies += selected[i];
    }

    return TwisterResult{ std::move(variants), parent, std::move(varies) };
}

struct AmazonProductResult {
    Offer offer;
    std::vector<std::string> variants;
};

inline AmazonProductResult parseAmazonProduct(std::string_view html, std::string_view host, std::string_view checkedAt) {
    auto document = Document::parse(html);
    auto asinEl = document.querySelector("#ASIN");
    std::string asin = asinEl ? asinEl->attr("value").value_or("") : "";
    if (asin.empty()) {
        auto canon = document.querySelector("link[rel=canonical]");
        if (canon) {
            asin = amazonAsinOf(canon->attr("href").value_or("")).value_or("");
        }
    }

    std::string name = cleanText(document.querySelector("#productTitle"));
    std::string priceText;
    auto priceNodes = document.querySelectorAll("#corePrice_feature_div .a-offscreen, #corePriceDisplay_desktop_feature_div .a-offscreen");
    for (const auto& pn : priceNodes) {
        std::string txt = cleanText(pn.text());
        if (txt.find_first_of("0123456789") != std::string::npos) {
            priceText = txt;
            break;
        }
    }
    auto amount = parsePrice(priceText);
    if (asin.empty() || name.empty() || !amount.has_value()) {
        throw std::runtime_error("amazon: no title or price on the product page (layout changed, or not available here?)");
    }

    std::map<std::string, slimr::Json> details;
    for (const auto& row : document.querySelectorAll("#productOverview_feature_div tr")) {
        auto cells = row.querySelectorAll("td");
        if (cells.size() >= 2) {
            std::string k = cleanText(cells[0].text());
            std::string v = cleanText(cells[1].text());
            if (!k.empty() && !v.empty()) details[k] = slimr::Json(v);
        }
    }

    for (const auto& row : document.querySelectorAll("#prodDetails tr, #productDetails_techSpec_section_1 tr, #productDetails_detailBullets_sections1 tr")) {
        auto th = row.querySelector("th");
        auto td = row.querySelector("td");
        if (th && td) {
            std::string k = cleanText(th->text());
            std::string v = cleanText(td->text());
            static const std::regex ltrRtl(R"(^[‎‏\s]+)");
            v = std::regex_replace(v, ltrRtl, "");
            if (!k.empty() && !v.empty() && !details.contains(k) && v.size() < 300) {
                details[k] = slimr::Json(v);
            }
        }
    }

    std::map<std::string, slimr::Json> spread;
    static const std::regex spreadRe(R"((\d+)\s+percent of reviews have\s+(\d)\s+star)");
    for (const auto& el : document.querySelectorAll("[aria-label*=\"percent of reviews have\"]")) {
        std::string aria = el.attr("aria-label").value_or("");
        std::smatch sm;
        if (std::regex_search(aria, sm, spreadRe)) {
            spread[sm[2].str() + " stars"] = slimr::Json(sm[1].str() + "%");
        }
    }
    if (!spread.empty()) {
        slimr::Json::object_t obj;
        for (auto& [k, v] : spread) obj[k] = std::move(v);
        details["Rating spread"] = slimr::Json(std::move(obj));
    }

    auto ppd = document.querySelector("#ppd, #desktop_buybox");
    if (ppd) {
        std::string ppdText = ppd->text();
        static const std::regex importRe(R"(Import Charges Included|Estimated Import Charges: [^\s]+)");
        std::smatch im;
        if (std::regex_search(ppdText, im, importRe)) {
            details["Import charges"] = slimr::Json(im[0].str());
        }
    }

    std::string crumbs = cleanText(document.querySelector("#wayfinding-breadcrumbs_feature_div"));
    std::optional<std::string> category;
    if (!crumbs.empty()) {
        std::istringstream iss(crumbs);
        std::string segment;
        std::string cat;
        while (std::getline(iss, segment, '\x9A')) { // check if contains ›
            // split by unicode ›
        }
        // Better regex split:
        static const std::regex crumbSep(R"(\s*›\s*)");
        std::sregex_token_iterator iter(crumbs.begin(), crumbs.end(), crumbSep, -1);
        std::sregex_token_iterator end;
        for (; iter != end; ++iter) {
            std::string part = cleanText(iter->str());
            if (!part.empty()) {
                if (!cat.empty()) cat += " > ";
                cat += part;
            }
        }
        if (!cat.empty()) category = cat;
    }

    auto twister = amazonTwisterOf(document);
    std::string byline = cleanText(document.querySelector("#bylineInfo"));
    static const std::regex bylinePrefix(R"(^(Visit the |Brand: ))");
    static const std::regex bylineSuffix(R"( Store$)");
    byline = std::regex_replace(byline, bylinePrefix, "");
    byline = std::regex_replace(byline, bylineSuffix, "");

    std::optional<std::string> brand;
    if (details.contains("Brand Name") && details["Brand Name"].is_string()) {
        brand = details["Brand Name"].get<std::string>();
    } else if (details.contains("Brand") && details["Brand"].is_string()) {
        brand = details["Brand"].get<std::string>();
    } else if (!byline.empty()) {
        brand = byline;
    }

    std::optional<std::string> model;
    for (const auto& k : { "Item model number", "Model Number", "Manufacturer reference", "Manufacturer Part Number" }) {
        if (details.contains(k) && details[k].is_string()) {
            model = details[k].get<std::string>();
            break;
        }
    }

    std::optional<std::string> gtin;
    for (const auto& k : { "EAN", "GTIN", "Global Trade Identification Number", "UPC", "Barcode" }) {
        if (details.contains(k) && details[k].is_string()) {
            gtin = details[k].get<std::string>();
            break;
        }
    }

    auto popover = document.querySelector("#acrPopover");
    double ratingVal = 0.0;
    bool hasRating = false;
    if (popover) {
        auto r = parsePrice(cleanText(popover->text()));
        if (r.has_value()) {
            ratingVal = *r;
            hasRating = true;
        }
    }

    auto count = amazonReviewCount(cleanText(document.querySelector("#acrCustomerReviewText")));
    std::string availability = cleanText(document.querySelector("#availability"));
    auto stock = parseStock(availability);

    std::string seller = cleanText(document.querySelector("#merchantInfoFeature_feature_div .offer-display-feature-text-message"));
    if (seller.empty()) seller = "unknown";

    auto ship = document.querySelector("[data-csa-c-content-id=\"DEXUnifiedCXPDM\"]");
    std::string deliveryLine;
    if (ship) {
        deliveryLine = ship->attr("data-csa-c-delivery-price").value_or("") + " delivery " +
                       ship->attr("data-csa-c-delivery-time").value_or("");
    } else {
        deliveryLine = cleanText(document.querySelector("#mir-layout-DELIVERY_BLOCK"));
    }
    auto delivery = amazonDeliveryOf(deliveryLine, "product", stock);

    Offer offer;
    offer.site = "amazon";
    offer.id = asin;
    offer.url = amazonProductUrl(host, asin);
    offer.name = name;
    if (brand.has_value()) offer.brand = *brand;
    if (model.has_value()) offer.model = *model;
    if (gtin.has_value()) offer.gtin = *gtin;
    if (category.has_value()) offer.category = *category;
    offer.seller = seller;
    offer.condition = amazonConditionOf(name + " " + availability);
    offer.price = Price{ *amount, amazonCurrencyOf(priceText, host), std::nullopt, true };
    if (stock.has_value()) offer.inStock = *stock;
    if (hasRating) {
        Rating r{ ratingVal, 5.0, count, std::nullopt };
        if (twister.variants.size() > 1) r.scope = "family";
        offer.rating = r;
    }
    offer.delivery = delivery;
    offer.details = std::move(details);
    if (twister.variants.size() > 1) {
        offer.family = Family{ twister.parent.value_or(asin), static_cast<int>(twister.variants.size()), twister.varies };
    }
    offer.checkedAt = std::string(checkedAt);

    return AmazonProductResult{ std::move(offer), std::move(twister.variants) };
}

inline std::vector<Offer> parseAmazonSellers(std::string_view html, const Offer& base, std::string_view host) {
    auto document = Document::parse(html);
    std::vector<Offer> out;

    auto els = document.querySelectorAll("#aod-pinned-offer, #aod-offer");
    for (const auto& el : els) {
        std::string symbol = cleanText(el.querySelector(".a-price-symbol"));
        std::string whole = cleanText(el.querySelector(".a-price-whole"));
        static const std::regex sep(R"([.,\s])");
        whole = std::regex_replace(whole, sep, "");
        std::string fraction = cleanText(el.querySelector(".a-price-fraction"));

        std::optional<double> amount;
        if (!whole.empty()) {
            amount = parsePrice(whole + "." + (fraction.empty() ? "00" : fraction));
        } else {
            amount = parsePrice(cleanText(el.querySelector(".a-offscreen")));
        }
        if (!amount.has_value()) continue;

        std::string seller = cleanText(el.querySelector("#aod-offer-soldBy .a-col-right a, #aod-offer-soldBy .a-col-right span"));
        if (seller.empty()) seller = "unknown";

        std::string heading = cleanText(el.querySelector("#aod-offer-heading"));
        for (char& c : heading) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        std::string condition = "unknown";
        if (heading.find("renewed") != std::string::npos || heading.find("refurbished") != std::string::npos) {
            condition = "refurbished";
        } else if (heading.find("used") != std::string::npos || heading.find("open box") != std::string::npos) {
            condition = "used";
        } else if (heading.find("new") != std::string::npos) {
            condition = "new";
        }

        std::string shipsFrom = cleanText(el.querySelector("#aod-offer-shipsFrom .a-col-right"));
        std::string rating = cleanText(el.querySelector("#aod-offer-seller-rating"));

        std::map<std::string, slimr::Json> details;
        if (!shipsFrom.empty()) details["Dispatches from"] = slimr::Json(shipsFrom);
        if (!rating.empty()) details["Seller rating"] = slimr::Json(rating);

        Offer o = base;
        o.family = std::nullopt;
        o.rating = std::nullopt;
        o.pickup = std::nullopt;
        o.seller = seller;
        o.condition = condition;
        o.price = Price{ *amount, amazonCurrencyOf(symbol, host), std::nullopt, true };
        o.inStock = true;
        o.delivery = amazonDeliveryOf(cleanText(el.querySelector(".aod-delivery-promise")), "listing");
        o.details = std::move(details);
        out.push_back(std::move(o));
    }
    return out;
}

inline std::vector<Review> amazonReviewsIn(const Document& document) {
    std::vector<Review> out;
    for (const auto& el : document.querySelectorAll("[data-hook=\"review\"]")) {
        auto starEl = el.querySelector("[data-hook=\"review-star-rating\"] .a-icon-alt, [data-hook=\"cmps-review-star-rating\"] .a-icon-alt");
        auto ratingOpt = parsePrice(cleanText(starEl));
        if (!ratingOpt.has_value()) continue;
        double rating = *ratingOpt;

        auto body = el.querySelector("[data-hook=\"review-body\"]");
        if (!body) continue;
        auto orig = body->querySelector(".cr-original-review-content");
        std::string reviewText = cleanText(orig ? orig : body);
        if (reviewText.empty()) continue;

        auto head = el.querySelector("[data-hook=\"review-title\"]");
        std::string title;
        if (head) {
            auto origHead = head->querySelector(".cr-original-review-content");
            if (origHead) {
                title = cleanText(origHead);
            } else {
                auto span = head->querySelector("span:not(.a-icon-alt)");
                if (span) title = cleanText(span);
            }
        }

        auto date = isoDate(cleanText(el.querySelector("[data-hook=\"review-date\"]")));

        std::string variant;
        auto formatStrip = el.querySelector("[data-hook=\"format-strip\"]");
        if (formatStrip) {
            std::vector<std::string> parts;
            for (const auto& child : formatStrip->childNodes()) {
                if (child.raw() && child.raw()->type == LXB_DOM_NODE_TYPE_TEXT) {
                    std::string t = cleanText(child.text());
                    if (!t.empty()) parts.push_back(t);
                }
            }
            for (size_t i = 0; i < parts.size(); ++i) {
                if (i > 0) variant += ", ";
                variant += parts[i];
            }
        }

        bool verified = el.querySelector("[data-hook=\"avp-badge\"]").has_value();

        Review r;
        r.rating = rating;
        r.scale = 5.0;
        if (!title.empty()) r.title = title;
        r.text = reviewText;
        if (date.has_value()) r.date = *date;
        if (verified) r.verified = true;
        if (!variant.empty()) r.variant = variant;
        out.push_back(std::move(r));
    }
    return out;
}

inline std::optional<std::string> amazonNextTokenIn(const Document& document) {
    auto btn = document.querySelector("[data-hook=\"show-more-button\"]");
    if (!btn) return std::nullopt;
    auto raw = btn->attr("data-reviews-state-param");
    if (!raw.has_value() || raw->empty()) return std::nullopt;
    auto parsed = slimr::parse_json<slimr::Json>(*raw);
    if (parsed && parsed->is_object() && parsed->contains("nextPageToken") && (*parsed)["nextPageToken"].is_string()) {
        std::string token = (*parsed)["nextPageToken"].get<std::string>();
        if (!token.empty()) return token;
    }
    return std::nullopt;
}

struct AmazonReviewPageResult {
    std::vector<Review> reviews;
    std::optional<int> total;
    std::optional<std::string> nextToken;
    std::optional<std::string> csrf;
};

inline AmazonReviewPageResult parseAmazonReviewPage(std::string_view html) {
    auto document = Document::parse(html);
    if (!document.querySelector("#cm_cr-review_list") && !document.querySelector("[data-hook='review']")) {
        static const std::regex authRe(R"(ap_email|signin|validateCaptcha)", std::regex::icase);
        std::string s(html);
        if (std::regex_search(s, authRe)) {
            throw std::runtime_error("amazon: the reviews page asked for sign-in or a CAPTCHA (not read)");
        }
        throw std::runtime_error("amazon: no review list on the reviews page (layout changed?)");
    }

    std::optional<int> total;
    auto countEl = document.querySelector("[data-hook=\"cr-filter-info-review-rating-count\"]");
    if (countEl) {
        static const std::regex totalRe(R"(([\d.,]+)\s+(?:matching\s+)?(?:customer\s+)?(?:reviews|ratings))", std::regex::icase);
        std::string txt = cleanText(countEl);
        std::smatch m;
        if (std::regex_search(txt, m, totalRe)) {
            std::string nStr = m[1].str();
            nStr.erase(std::remove(nStr.begin(), nStr.end(), ','), nStr.end());
            nStr.erase(std::remove(nStr.begin(), nStr.end(), '.'), nStr.end());
            try {
                total = std::stoi(nStr);
            } catch (...) {}
        }
    }

    std::optional<std::string> csrf;
    auto stateObj = document.querySelector("#cr-state-object");
    if (stateObj) {
        auto stateVal = stateObj->attr("data-state");
        if (stateVal.has_value()) {
            auto parsed = slimr::parse_json<slimr::Json>(*stateVal);
            if (parsed && parsed->is_object() && parsed->contains("reviewsCsrfToken") && (*parsed)["reviewsCsrfToken"].is_string()) {
                csrf = urlDecode((*parsed)["reviewsCsrfToken"].get<std::string>());
            }
        }
    }

    auto reviews = amazonReviewsIn(document);
    auto nextToken = amazonNextTokenIn(document);

    return AmazonReviewPageResult{ std::move(reviews), total, std::move(nextToken), std::move(csrf) };
}

struct AmazonReviewAjaxResult {
    std::vector<Review> reviews;
    std::optional<std::string> nextToken;
};

inline AmazonReviewAjaxResult parseAmazonReviewAjax(std::string_view body) {
    std::string html;
    std::string str(body);
    std::string delimiter = "&&&";
    size_t pos = 0;
    while (pos < str.size()) {
        size_t next = str.find(delimiter, pos);
        std::string part = (next == std::string::npos) ? str.substr(pos) : str.substr(pos, next - pos);
        part = cleanText(part);
        if (!part.empty()) {
            auto parsed = slimr::parse_json<slimr::Json>(part);
            if (parsed && parsed->is_array()) {
                auto& arr = parsed->get<slimr::Json::array_t>();
                if (arr.size() >= 3 && arr[0].is_string()) {
                    std::string op = arr[0].get<std::string>();
                    if ((op == "append" || op == "update") && arr[2].is_string()) {
                        html += arr[2].get<std::string>();
                    }
                }
            }
        }
        if (next == std::string::npos) break;
        pos = next + delimiter.size();
    }

    auto document = Document::parse("<body>" + html + "</body>");
    auto reviews = amazonReviewsIn(document);
    auto nextToken = amazonNextTokenIn(document);
    return AmazonReviewAjaxResult{ std::move(reviews), std::move(nextToken) };
}

class AmazonAdapter : public SiteAdapter {
public:
    std::string id() const override { return "amazon"; }
    bool deliveryOnly() const override { return true; }

    std::string matchHint() const override {
        return "Amazon matches loosely and ranks by its own relevance: use the product type plus brand or model; sponsored rows are dropped.";
    }

    SearchResult search(SiteContext& ctx, const SearchQuery& q) override {
        const std::string HOST = "www.amazon.ie";
        std::string url = "https://" + HOST + "/s?k=" + urlEncode(q.query);
        if (q.filters.minPrice.has_value()) {
            url += "&low-price=" + std::to_string(static_cast<int>(std::floor(*q.filters.minPrice)));
        }
        if (q.filters.maxPrice.has_value()) {
            url += "&high-price=" + std::to_string(static_cast<int>(std::ceil(*q.filters.maxPrice)));
        }
        std::string html = ctx.fetchText(url);
        return parseAmazonSearch(html, HOST, "2026-10-15T00:00:00Z");
    }

    std::vector<Offer> offer(SiteContext& ctx, const OfferRef& ref, const OfferOptions& options) override {
        std::string asin = ref.id.value_or("");
        if (asin.empty() && ref.url.has_value()) {
            asin = amazonAsinOf(*ref.url).value_or("");
        }
        if (asin.empty()) {
            throw std::runtime_error("amazon: give an ASIN (10 characters) or a product URL");
        }
        std::string host = "www.amazon.ie";
        if (ref.url.has_value()) {
            CURLU* u = curl_url();
            if (u) {
                if (curl_url_set(u, CURLUPART_URL, ref.url->c_str(), 0) == CURLUE_OK) {
                    char* h = nullptr;
                    curl_url_get(u, CURLUPART_HOST, &h, 0);
                    if (h) {
                        host = h;
                        curl_free(h);
                    }
                }
                curl_url_cleanup(u);
            }
        }
        std::string checkedAt = "2026-10-15T00:00:00Z";
        std::string html = fetchProductHtml(ctx, host, asin);
        auto main = parseAmazonProduct(html, host, checkedAt);
        std::vector<Offer> out = { main.offer };

        if (options.sellers) {
            std::string aodUrl = "https://" + host + "/gp/product/ajax/aodAjaxMain/ref=dp_aod_ALL_mbc?asin=" + asin + "&pc=dp&experienceId=aodAjaxMain";
            std::string aod = ctx.fetchText(aodUrl);
            auto sellers = parseAmazonSellers(aod, main.offer, host);
            for (auto& s : sellers) {
                if (!(s.seller == main.offer.seller && s.price.amount == main.offer.price.amount)) {
                    out.push_back(std::move(s));
                }
            }
        }

        if (options.family) {
            const size_t MAX_VARIANTS = 8;
            size_t count = 0;
            for (const auto& variant : main.variants) {
                if (variant == asin) continue;
                if (++count > MAX_VARIANTS) break;
                std::string varHtml = fetchProductHtml(ctx, host, variant);
                out.push_back(parseAmazonProduct(varHtml, host, checkedAt).offer);
            }
        }
        return out;
    }

    ReviewsResult reviews(SiteContext& ctx, const OfferRef& ref, const ReviewQuery& q) override {
        std::string asin = ref.id.value_or("");
        if (asin.empty() && ref.url.has_value()) {
            asin = amazonAsinOf(*ref.url).value_or("");
        }
        if (asin.empty()) {
            throw std::runtime_error("amazon: give an ASIN (10 characters) or a product URL");
        }

        struct Filter {
            std::string filterByStar;
            std::string sortBy;
        };
        Filter f;
        if (q.sort == "lowest") {
            f.filterByStar = "critical";
            f.sortBy = "";
        } else if (q.sort == "newest") {
            f.filterByStar = "";
            f.sortBy = "recent";
        } else {
            f.filterByStar = "";
            f.sortBy = "helpful";
        }

        const std::string HOST = "www.amazon.ie";
        std::string first = (q.sort == "lowest")
            ? "filterByStar=critical"
            : "reviewerType=all_reviews&sortBy=" + f.sortBy;

        std::string html = ctx.fetchText("https://" + HOST + "/product-reviews/" + asin + "/?" + first);
        auto page = parseAmazonReviewPage(html);
        std::vector<Review> reviews = std::move(page.reviews);
        auto token = page.nextToken;

        const int MAX_REVIEW_PAGES = 20;
        const int REVIEW_PAGE_SIZE = 10;
        for (int n = 2; static_cast<int>(reviews.size()) < q.limit && token.has_value() && n <= MAX_REVIEW_PAGES; ++n) {
            if (!page.csrf.has_value()) {
                throw std::runtime_error("amazon: no CSRF token on the reviews page (layout changed?)");
            }
            std::string reftag = "cm_cr_arp_d_paging_btm_next_" + std::to_string(n);
            std::string body = "sortBy=" + urlEncode(f.sortBy) +
                "&reviewerType=all_reviews&formatType=&mediaType=&filterByStar=" + urlEncode(f.filterByStar) +
                "&filterByAge=&pageNumber=" + std::to_string(n) +
                "&filterByLanguage=&filterByKeyword=&shouldAppend=true&deviceType=desktop&canShowIntHeader=false" +
                "&reftag=" + reftag +
                "&pageSize=" + std::to_string(REVIEW_PAGE_SIZE) +
                "&asin=" + asin +
                "&scope=reviewsAjax0" +
                "&nextPageToken=" + urlEncode(*token);

            FetchInit init;
            init.method = "POST";
            init.headers["content-type"] = "application/x-www-form-urlencoded;charset=UTF-8";
            init.headers["anti-csrftoken-a2z"] = *page.csrf;
            init.headers["x-requested-with"] = "XMLHttpRequest";
            init.body = body;

            std::string res = ctx.fetchText("https://" + HOST + "/portal/customer-reviews/ajax/reviews/get/ref=" + reftag, init);
            auto next = parseAmazonReviewAjax(res);
            for (auto& r : next.reviews) reviews.push_back(std::move(r));
            token = next.reviews.empty() ? std::nullopt : next.nextToken;
        }

        if (static_cast<int>(reviews.size()) > q.limit) {
            reviews.resize(q.limit);
        }
        return ReviewsResult{ std::move(reviews), page.total };
    }

private:
    std::string fetchProductHtml(SiteContext& ctx, const std::string& host, const std::string& asin) {
        if (host == "www.amazon.ie") {
            return ctx.fetchText("https://" + host + "/dp/" + asin);
        }
        ctx.gotoUrl("https://" + host + "/dp/" + asin);
        return ctx.html();
    }
};

} // namespace shop::sites
