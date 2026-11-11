#pragma once

#include "site_utils.hpp"

namespace shop::sites {

inline std::optional<std::string> bingMerchantUrl(std::string_view href) {
    std::string target(href);
    std::string u = getQueryParam(href, "u");
    if (!u.empty()) {
        std::string dec = urlDecode(base64UrlDecode(u));
        if (!dec.empty()) target = dec;
    }
    std::string dest = getQueryParam(target, "ds_dest_url");
    if (!dest.empty()) {
        target = dest;
    }

    if (!target.starts_with("https://")) return std::nullopt;

    CURLU* curlUrl = curl_url();
    if (!curlUrl) return std::nullopt;

    if (curl_url_set(curlUrl, CURLUPART_URL, target.c_str(), 0) != CURLUE_OK) {
        curl_url_cleanup(curlUrl);
        return std::nullopt;
    }
    curl_url_set(curlUrl, CURLUPART_FRAGMENT, nullptr, 0);

    char* hostPtr = nullptr;
    if (curl_url_get(curlUrl, CURLUPART_HOST, &hostPtr, 0) != CURLUE_OK || !hostPtr) {
        curl_url_cleanup(curlUrl);
        return std::nullopt;
    }
    std::string host = hostPtr;
    curl_free(hostPtr);

    std::string hostLower = host;
    for (char& c : hostLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (hostLower == "bing.com" || hostLower.ends_with(".bing.com")) {
        curl_url_cleanup(curlUrl);
        return std::nullopt;
    }

    bool isEbay = (hostLower.find("ebay.") != std::string::npos);
    if (isEbay) {
        curl_url_set(curlUrl, CURLUPART_QUERY, nullptr, 0);
    } else {
        char* queryPtr = nullptr;
        if (curl_url_get(curlUrl, CURLUPART_QUERY, &queryPtr, 0) == CURLUE_OK && queryPtr) {
            std::string query = queryPtr;
            curl_free(queryPtr);

            static const std::regex trackingRe(
                R"(^(utm_|mk|msclkid$|gclid$|gclsrc$|adp$|ac$|country$|chn$|norover$|itemid$|targetid$|device$))",
                std::regex::icase);

            std::vector<std::pair<std::string, std::string>> kept;
            std::istringstream iss(query);
            std::string param;
            while (std::getline(iss, param, '&')) {
                if (param.empty()) continue;
                size_t eq = param.find('=');
                std::string k = (eq == std::string::npos) ? param : param.substr(0, eq);
                std::string v = (eq == std::string::npos) ? "" : param.substr(eq + 1);
                if (!std::regex_search(k, trackingRe)) {
                    kept.emplace_back(std::move(k), std::move(v));
                }
            }

            if (kept.empty()) {
                curl_url_set(curlUrl, CURLUPART_QUERY, nullptr, 0);
            } else {
                std::string newQuery;
                for (size_t i = 0; i < kept.size(); ++i) {
                    if (i > 0) newQuery += "&";
                    newQuery += kept[i].first;
                    if (!kept[i].second.empty()) {
                        newQuery += "=" + kept[i].second;
                    }
                }
                curl_url_set(curlUrl, CURLUPART_QUERY, newQuery.c_str(), 0);
            }
        }
    }

    char* fullPtr = nullptr;
    std::string out;
    if (curl_url_get(curlUrl, CURLUPART_URL, &fullPtr, 0) == CURLUE_OK && fullPtr) {
        out = fullPtr;
        curl_free(fullPtr);
    }
    curl_url_cleanup(curlUrl);

    if (out.empty()) return std::nullopt;
    return out;
}

inline std::string bingFullText(const std::optional<Node>& el) {
    if (!el || !*el) return "";
    auto titleEl = el->querySelector("[title]");
    if (titleEl) {
        auto t = titleEl->attr("title");
        if (t.has_value() && !t->empty()) return cleanText(*t);
    }
    return cleanText(el->text());
}

inline SearchResult parseBingSearch(std::string_view html, std::string_view checkedAt, std::string_view query) {
    auto document = Document::parse(html);
    std::vector<Offer> offers;

    auto cards = document.querySelectorAll(".br-gOffCard");
    for (const auto& card : cards) {
        auto offerIdOpt = card.attr("data-offerid");
        if (!offerIdOpt.has_value() || offerIdOpt->empty()) continue;
        std::string offerId = *offerIdOpt;

        auto link = card.querySelector("a.br-offLink");
        std::string href = link ? link->attr("href").value_or("") : "";
        auto urlOpt = bingMerchantUrl(href);
        if (!urlOpt.has_value()) continue;
        std::string url = *urlOpt;

        std::string priceText = cleanText(card.querySelector(".br-price"));
        std::string currency;
        std::string amountStr;
        if (priceText.starts_with("€")) {
            currency = "EUR";
            amountStr = priceText.substr(std::string_view("€").size());
        } else if (priceText.starts_with("£")) {
            currency = "GBP";
            amountStr = priceText.substr(std::string_view("£").size());
        } else if (priceText.starts_with("$")) {
            currency = "USD";
            amountStr = priceText.substr(1);
        } else {
            continue;
        }

        auto amount = parsePrice(amountStr);
        if (!amount.has_value()) continue;

        std::string name = bingFullText(card.querySelector(".br-offTtl"));
        if (name.empty()) continue;

        std::optional<Rating> rating;
        auto ratingEl = card.querySelector(".sa_rating");
        if (ratingEl) {
            static const std::regex ratingRe(R"(([\d.]+)\s*·\s*(\d+))");
            std::string rTxt = cleanText(ratingEl->text());
            std::smatch rm;
            if (std::regex_search(rTxt, rm, ratingRe)) {
                try {
                    rating = Rating{ std::stod(rm[1].str()), 5.0, std::stoi(rm[2].str()), std::nullopt };
                } catch (...) {}
            }
        }

        bool free = card.querySelector(".sa_fshp").has_value();

        std::map<std::string, slimr::Json> details;
        if (link) {
            std::string rel = link->attr("rel").value_or("");
            if (rel.find("sponsored") != std::string::npos) {
                details["Sponsored"] = slimr::Json(true);
            }
        }
        auto lblEl = card.querySelector(".br-lblTxt");
        if (lblEl) {
            std::string lbl = cleanText(lblEl->attr("title"));
            if (!lbl.empty()) {
                details["Special offer"] = slimr::Json(lbl);
            }
        }

        std::string seller = bingFullText(card.querySelector(".br-offSlrTxt"));
        if (seller.empty()) seller = "unknown";

        Offer offer;
        offer.site = "bing";
        offer.id = offerId + ":" + std::string(query);
        offer.url = url;
        offer.name = name;
        offer.seller = seller;
        offer.condition = "unknown";
        offer.price = Price{ *amount, currency, std::nullopt, true };
        if (rating.has_value()) offer.rating = rating;
        offer.delivery = free
            ? Delivery{ "available", std::nullopt, std::nullopt, 0.0, "listing" }
            : Delivery{ "unknown", std::nullopt, std::nullopt, std::nullopt, "listing" };
        offer.details = std::move(details);
        offer.checkedAt = std::string(checkedAt);
        offers.push_back(std::move(offer));
    }
    return SearchResult{ std::move(offers), std::nullopt, {} };
}

class BingAdapter : public SiteAdapter {
public:
    std::string id() const override { return "bing"; }
    bool deliveryOnly() const override { return true; }

    std::string matchHint() const override {
        return "Bing Shop matches loosely across merchants and mostly lists paid placements; use the product name and model, then verify on the merchant's site.";
    }

    SearchResult search(SiteContext& ctx, const SearchQuery& q) override {
        std::string url = "https://www.bing.com/shop?q=" + urlEncode(q.query) + "&cc=IE&setlang=en";
        FetchInit init;
        init.document = true;
        std::string html = ctx.fetchText(url, init);
        return parseBingSearch(html, "2026-10-15T00:00:00Z", q.query);
    }

    std::vector<Offer> offer(SiteContext& ctx, const OfferRef& ref, const OfferOptions& options) override {
        (void)options;
        std::string id = ref.id.value_or("");
        size_t split = id.find(':');
        if (split == std::string::npos || split == 0) {
            throw std::runtime_error("bing: pass the id from a search row (`<offerid>:<query>`); Bing has no product page, so re-check on the merchant's site");
        }
        std::string query = id.substr(split + 1);
        std::string url = "https://www.bing.com/shop?q=" + urlEncode(query) + "&cc=IE&setlang=en";
        FetchInit init;
        init.document = true;
        std::string html = ctx.fetchText(url, init);
        auto res = parseBingSearch(html, "2026-10-15T00:00:00Z", query);
        for (auto& o : res.offers) {
            if (o.id == id) {
                return { std::move(o) };
            }
        }
        throw std::runtime_error("bing: the listing " + id.substr(0, split) + " is no longer in Bing Shop results for \"" + query + "\": re-check on the merchant's site");
    }
};

} // namespace shop::sites
