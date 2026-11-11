#pragma once

#include "core/adapter.hpp"
#include "core/dom.hpp"
#include "curl.hpp"
#include "json.hpp"

#include <algorithm>
#include <cmath>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace shop::sites {

using namespace shop::core;

namespace very_internal {

inline const std::string BASE = "https://www.very.ie/";
inline const std::string BV = "https://apps.bazaarvoice.com/bfd/v1/clients/littlewoods-ie/api-products/cv2/resources/data/reviews.json";
inline const Delivery SITE_DELIVERY{
    .status = "unknown",
    .minDays = 3,
    .maxDays = 7,
    .cost = 3.99,
    .source = "site default"
};

inline std::optional<std::string> idOf(std::string_view url) {
    static const std::regex idRe(R"(/(\d+)\.prd)", std::regex::icase);
    std::string s(url);
    std::smatch m;
    if (std::regex_search(s, m, idRe)) {
        return m[1].str();
    }
    return std::nullopt;
}

} // namespace very_internal

inline SearchResult parseVerySearch(std::string_view html, std::string_view checkedAt) {
    using namespace very_internal;
    auto doc = Document::parse(html);
    std::vector<Offer> offers;

    auto cards = doc.querySelectorAll("[data-testid=\"gallery-product-card\"]");
    for (const auto& card : cards) {
        auto idAttr = card.attr("data-cnstrc-item-id");
        auto a = card.querySelector("a");
        auto hrefAttr = a ? a->attr("href") : std::nullopt;
        auto priceAttr = card.attr("data-cnstrc-item-price");
        std::optional<double> amount = priceAttr ? parsePrice(*priceAttr) : std::nullopt;

        if (!idAttr || !hrefAttr || !amount || *amount <= 0) continue;

        std::string name = text(card.attr("data-cnstrc-item-name").value_or(""));

        auto priceBasic = card.querySelector("[data-testid=\"fuse-product-card__price__basic\"]");
        std::string priceText = priceBasic ? text(priceBasic->text()) : "";

        auto prevPriceEl = card.querySelector("[data-testid=\"fuse-product-card__price__prev\"]");
        std::optional<double> was;
        if (prevPriceEl) {
            std::string prevText = text(prevPriceEl->text());
            static const std::regex regWasRe(R"(^Regular price was\s*)", std::regex::icase);
            prevText = std::regex_replace(prevText, regWasRe, "");
            static const std::regex fromRe(R"(^From\s*)", std::regex::icase);
            prevText = std::regex_replace(prevText, fromRe, "");
            was = parsePrice(prevText);
        }

        std::optional<Rating> rating;
        auto ratingEl = card.querySelector("[data-rating]");
        double rVal = 0.0;
        if (ratingEl) {
            auto ratAttr = ratingEl->attr("data-rating");
            if (ratAttr) {
                try { rVal = std::stod(*ratAttr); } catch (...) {}
            }
        }
        std::optional<int> reviewCount;
        std::string cardText = card.text();
        static const std::regex revRe(R"((\d+) reviews?)", std::regex::icase);
        std::smatch rm;
        if (std::regex_search(cardText, rm, revRe)) {
            reviewCount = std::stoi(rm[1].str());
        }
        if (rVal > 0) {
            rating = Rating{.value = rVal, .scale = 5.0, .count = reviewCount};
        }

        std::optional<std::string> brand;
        auto brandEl = card.querySelector("[data-testid=\"fuse-product-card__brand\"]");
        if (brandEl) {
            std::string b = text(brandEl->text());
            if (!b.empty()) brand = std::move(b);
        }

        auto quantity = parseQuantity(name);
        std::map<std::string, slimr::Json> details;
        static const std::regex fromCheck(R"(from)", std::regex::icase);
        if (std::regex_search(priceText, fromCheck)) {
            details["Price"] = "From price: variants (sizes, colours) start here";
        }

        Offer o;
        o.site = "very";
        o.id = *idAttr;
        o.url = canonicalUrl(*hrefAttr, BASE);
        o.name = std::move(name);
        o.brand = std::move(brand);
        o.price = Price{
            .amount = *amount,
            .currency = "EUR",
            .was = (was.has_value() && *was > *amount) ? was : std::nullopt,
            .taxIncluded = true
        };
        o.quantity = std::move(quantity);
        o.rating = std::move(rating);
        o.delivery = SITE_DELIVERY;
        o.details = std::move(details);
        o.checkedAt = std::string(checkedAt);
        offers.push_back(std::move(o));
    }

    std::optional<int> total;
    auto topEl = doc.querySelector("#results-header-top");
    if (topEl) {
        std::string topText = topEl->text();
        static const std::regex totalRe(R"(\(of (\d+)\))");
        std::smatch tm;
        if (std::regex_search(topText, tm, totalRe)) {
            total = std::stoi(tm[1].str());
        }
    }

    return SearchResult{.offers = std::move(offers), .total = total};
}

inline Offer parseVeryProduct(std::string_view html, std::string_view checkedAt) {
    using namespace very_internal;
    auto doc = Document::parse(html);
    auto blocks = doc.jsonLd();
    auto groupOpt = findLd(blocks, "ProductGroup");
    if (!groupOpt) {
        groupOpt = findLd(blocks, "Product");
    }
    if (!groupOpt || !groupOpt->is_object()) {
        throw std::runtime_error("very: no ProductGroup JSON-LD on the page (layout changed?)");
    }
    const auto& group = *groupOpt;

    struct VarInfo {
        slimr::Json v;
        slimr::Json o;
    };
    std::vector<VarInfo> variants;

    if (group.contains("hasVariant") && group["hasVariant"].is_array() && !group["hasVariant"].get<slimr::Json::array_t>().empty()) {
        for (const auto& v : group["hasVariant"].get<slimr::Json::array_t>()) {
            slimr::Json offer;
            if (v.is_object() && v.contains("offers")) {
                if (v["offers"].is_array() && !v["offers"].get<slimr::Json::array_t>().empty()) {
                    offer = v["offers"].get<slimr::Json::array_t>()[0];
                } else if (v["offers"].is_object()) {
                    offer = v["offers"];
                }
            }
            variants.push_back(VarInfo{.v = v, .o = std::move(offer)});
        }
    } else {
        slimr::Json offer;
        if (group.contains("offers")) {
            if (group["offers"].is_array() && !group["offers"].get<slimr::Json::array_t>().empty()) {
                offer = group["offers"].get<slimr::Json::array_t>()[0];
            } else if (group["offers"].is_object()) {
                offer = group["offers"];
            }
        }
        variants.push_back(VarInfo{.v = group, .o = std::move(offer)});
    }

    std::vector<double> prices;
    for (const auto& vi : variants) {
        if (vi.o.is_object() && vi.o.contains("price")) {
            std::optional<double> p;
            if (vi.o["price"].is_string()) p = parsePrice(vi.o["price"].get<std::string>());
            else if (vi.o["price"].is_number()) p = vi.o["price"].get<double>();
            if (p) prices.push_back(*p);
        }
    }
    if (prices.empty()) {
        throw std::runtime_error("very: no price in the product JSON-LD");
    }

    const auto& first = variants[0];
    double amount = prices[0];
    if (first.o.is_object() && first.o.contains("price")) {
        std::optional<double> p;
        if (first.o["price"].is_string()) p = parsePrice(first.o["price"].get<std::string>());
        else if (first.o["price"].is_number()) p = first.o["price"].get<double>();
        if (p) amount = *p;
    }

    auto prevEl = doc.querySelector("[data-testid=\"product-price__prev\"]");
    std::optional<double> was;
    if (prevEl) {
        std::string prevText = text(prevEl->text());
        static const std::regex regWasRe(R"(^Regular price was\s*)", std::regex::icase);
        prevText = std::regex_replace(prevText, regWasRe, "");
        was = parsePrice(prevText);
    }

    std::string returnPolicyCategory;
    if (first.o.is_object() && first.o.contains("hasMerchantReturnPolicy") && first.o["hasMerchantReturnPolicy"].is_object()) {
        auto& retObj = first.o["hasMerchantReturnPolicy"];
        if (retObj.contains("returnPolicyCategory") && retObj["returnPolicyCategory"].is_string()) {
            returnPolicyCategory = retObj["returnPolicyCategory"].get<std::string>();
        }
    }

    bool inStock = false;
    bool known = false;
    for (const auto& vi : variants) {
        if (vi.o.is_object() && vi.o.contains("availability")) {
            known = true;
            std::string av = vi.o["availability"].is_string() ? vi.o["availability"].get<std::string>() : "";
            static const std::regex inStockRe(R"(InStock)", std::regex::icase);
            if (std::regex_search(av, inStockRe)) {
                inStock = true;
                break;
            }
        }
    }

    std::optional<Rating> rating;
    if (group.contains("aggregateRating") && group["aggregateRating"].is_object()) {
        auto& agg = group["aggregateRating"];
        double rVal = 0.0;
        if (agg.contains("ratingValue")) {
            if (agg["ratingValue"].is_string()) {
                try { rVal = std::stod(agg["ratingValue"].get<std::string>()); } catch (...) {}
            } else if (agg["ratingValue"].is_number()) {
                rVal = agg["ratingValue"].get<double>();
            }
        }
        std::optional<int> rCount;
        if (agg.contains("reviewCount")) {
            if (agg["reviewCount"].is_string()) {
                try { rCount = std::stoi(agg["reviewCount"].get<std::string>()); } catch (...) {}
            } else if (agg["reviewCount"].is_number()) {
                rCount = static_cast<int>(agg["reviewCount"].get<double>());
            }
        }
        if (rVal > 0) {
            rating = Rating{.value = rVal, .scale = 5.0, .count = rCount};
        }
    }

    std::string name = group.contains("name") && group["name"].is_string() ? text(group["name"].get<std::string>()) : "";
    std::string rawUrl;
    if (group.contains("url") && group["url"].is_string()) {
        rawUrl = group["url"].get<std::string>();
    } else if (first.v.is_object() && first.v.contains("url") && first.v["url"].is_string()) {
        rawUrl = first.v["url"].get<std::string>();
    }

    auto quantity = parseQuantity(name);
    std::optional<std::string> model;
    if (first.v.is_object() && first.v.contains("mpn") && first.v["mpn"].is_string()) {
        std::string m = text(first.v["mpn"].get<std::string>());
        if (!m.empty()) model = std::move(m);
    }

    std::map<std::string, slimr::Json> details;
    if (first.v.is_object() && first.v.contains("sku") && first.v["sku"].is_string()) {
        std::string sku = text(first.v["sku"].get<std::string>());
        if (!sku.empty()) details["Item number"] = sku;
    }
    if (model) {
        details["Model"] = *model;
    }
    if (first.v.is_object() && first.v.contains("color") && first.v["color"].is_string()) {
        std::string col = text(first.v["color"].get<std::string>());
        if (!col.empty()) details["Colour"] = col;
    }

    if (variants.size() > 1) {
        double lo = *std::min_element(prices.begin(), prices.end());
        double hi = *std::max_element(prices.begin(), prices.end());
        auto formatP = [](double v) {
            std::string s = std::to_string(v);
            while (s.size() > 1 && s.back() == '0') s.pop_back();
            if (!s.empty() && s.back() == '.') s.pop_back();
            return s;
        };
        if (lo == hi) {
            details["Variants"] = std::to_string(variants.size()) + " variants, all " + formatP(lo) + " EUR";
        } else {
            details["Variants"] = std::to_string(variants.size()) + " variants, " + formatP(lo) + " to " + formatP(hi) + " EUR";
        }
    }

    static const std::regex notPermittedRe(R"(NotPermitted)", std::regex::icase);
    if (!returnPolicyCategory.empty() && std::regex_search(returnPolicyCategory, notPermittedRe)) {
        details["Returns"] = "Non returnable once installed or connected";
    }

    if (group.contains("description") && group["description"].is_string()) {
        std::string desc = text(group["description"].get<std::string>());
        static const std::regex tagRe(R"(<[^>]+>)");
        desc = std::regex_replace(desc, tagRe, " ");
        desc = text(desc);
        if (!desc.empty()) {
            if (desc.size() > 600) desc = desc.substr(0, 600);
            details["Description"] = desc;
        }
    }

    std::string id = idOf(rawUrl).value_or("");
    if (id.empty() && group.contains("productGroupID") && group["productGroupID"].is_string()) {
        id = text(group["productGroupID"].get<std::string>());
    }

    std::optional<std::string> brand;
    if (group.contains("brand") && group["brand"].is_object() && group["brand"].contains("name") && group["brand"]["name"].is_string()) {
        brand = text(group["brand"]["name"].get<std::string>());
    }

    std::optional<std::string> gtin;
    if (group.contains("gtin") && group["gtin"].is_string()) {
        std::string g = text(group["gtin"].get<std::string>());
        if (!g.empty()) gtin = std::move(g);
    }

    std::string currency = "EUR";
    if (first.o.is_object() && first.o.contains("priceCurrency") && first.o["priceCurrency"].is_string()) {
        std::string pc = text(first.o["priceCurrency"].get<std::string>());
        if (!pc.empty()) currency = std::move(pc);
    }

    Delivery delivery = SITE_DELIVERY;
    if (known && !inStock) {
        delivery.status = "unavailable";
    }

    Offer o;
    o.site = "very";
    o.id = std::move(id);
    o.url = canonicalUrl(rawUrl, BASE);
    o.name = std::move(name);
    o.brand = std::move(brand);
    o.model = std::move(model);
    o.gtin = std::move(gtin);
    o.price = Price{
        .amount = amount,
        .currency = std::move(currency),
        .was = (was.has_value() && *was > amount) ? was : std::nullopt,
        .taxIncluded = true
    };
    o.quantity = std::move(quantity);
    if (known) o.inStock = inStock;
    o.rating = std::move(rating);
    o.delivery = std::move(delivery);
    o.details = std::move(details);
    o.checkedAt = std::string(checkedAt);
    return o;
}

inline ReviewsResult parseVeryReviews(const slimr::Json& json) {
    const slimr::Json* root = &json;
    if (json.is_object() && json.contains("response") && json["response"].is_object()) {
        root = &json["response"];
    }

    std::vector<Review> reviews;
    if (root->is_object() && root->contains("Results") && (*root)["Results"].is_array()) {
        for (const auto& r : (*root)["Results"].get<slimr::Json::array_t>()) {
            if (!r.is_object()) continue;
            if (!r.contains("Rating") || !r["Rating"].is_number() ||
                !r.contains("ReviewText") || !r["ReviewText"].is_string()) {
                continue;
            }
            double rating = r["Rating"].get<double>();
            double scale = 5.0;
            if (r.contains("RatingRange") && r["RatingRange"].is_number()) {
                scale = r["RatingRange"].get<double>();
            }
            std::optional<std::string> title;
            if (r.contains("Title") && r["Title"].is_string()) {
                std::string t = text(r["Title"].get<std::string>());
                if (!t.empty()) title = std::move(t);
            }
            std::string reviewText = text(r["ReviewText"].get<std::string>());
            std::optional<std::string> date;
            if (r.contains("SubmissionTime") && r["SubmissionTime"].is_string()) {
                std::string s = r["SubmissionTime"].get<std::string>();
                if (s.size() >= 10) date = s.substr(0, 10);
            }
            std::optional<bool> verified;
            if (r.contains("Badges") && r["Badges"].is_object()) {
                verified = r["Badges"].contains("verifiedPurchaser");
            }
            std::optional<bool> syndicated;
            if (r.contains("IsSyndicated") && r["IsSyndicated"].is_boolean()) {
                syndicated = r["IsSyndicated"].get<bool>();
            }

            reviews.push_back(Review{
                .rating = rating,
                .scale = scale,
                .title = std::move(title),
                .text = std::move(reviewText),
                .date = std::move(date),
                .verified = verified,
                .variant = std::nullopt,
                .syndicated = syndicated
            });
        }
    }

    std::optional<int> total;
    if (root->is_object() && root->contains("TotalResults") && (*root)["TotalResults"].is_number()) {
        total = static_cast<int>((*root)["TotalResults"].get<double>());
    }
    return ReviewsResult{.reviews = std::move(reviews), .total = total};
}

class VeryAdapter : public SiteAdapter {
public:
    std::string id() const override { return "very"; }
    bool deliveryOnly() const override { return true; }
    std::string matchHint() const override {
        return "Constructor.io search: matches loosely and returns unrelated \"best guess\" products for nonsense; "
               "use the product type and brand (\"jbl headphones\").";
    }

    SearchResult search(SiteContext& ctx, const SearchQuery& query) override {
        std::string trimmed = text(query.query);
        std::string url = very_internal::BASE + "e/q/" + slimr::url_encode(trimmed) + ".end";
        std::string html = ctx.fetchText(url);
        return parseVerySearch(html, "");
    }

    std::vector<Offer> offer(SiteContext& ctx, const OfferRef& ref, const OfferOptions& options) override {
        (void)options;
        std::string url = ref.url ? canonicalUrl(*ref.url, very_internal::BASE) : "";
        if (url.empty() && ref.id) {
            url = very_internal::BASE + "p/" + slimr::url_encode(*ref.id) + ".prd";
        }
        if (url.empty()) {
            throw std::runtime_error("very: give an id or url");
        }
        std::string html = ctx.fetchText(url);
        return {parseVeryProduct(html, "")};
    }

    ReviewsResult reviews(SiteContext& ctx, const OfferRef& ref, const ReviewQuery& query) override {
        std::string id = ref.id.value_or("");
        if (id.empty() && ref.url) {
            id = very_internal::idOf(*ref.url).value_or("");
        }
        if (id.empty()) {
            throw std::runtime_error("very: give the product id");
        }

        std::string sortParam = "rating:asc";
        if (query.sort == "newest") sortParam = "submissiontime:desc";
        else if (query.sort == "helpful") sortParam = "helpfulness:desc";

        int limit = std::min(query.limit, 100);
        std::string url = very_internal::BV +
            "?resource=reviews&action=REVIEWS_N_STATS&filteredstats=reviews&Stats=Reviews"
            "&limit=" + std::to_string(limit) +
            "&offset=0&sort=" + slimr::url_encode(sortParam) +
            "&apiversion=5.5&displaycode=19737-en_ie"
            "&filter=productid%3Aeq%3A" + slimr::url_encode(id) +
            "&filter=contentlocale%3Aeq%3Aen_GB%2Cen_IE"
            "&filter=isratingsonly%3Aeq%3Afalse";

        FetchInit init;
        init.headers["bv-bfd-token"] = "19737,main_site,en_IE";
        slimr::Json json = ctx.fetchJson(url, init);
        return parseVeryReviews(json);
    }
};

} // namespace shop::sites
