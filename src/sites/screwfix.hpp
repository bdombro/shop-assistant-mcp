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

namespace screwfix_internal {

inline const std::string BASE = "https://www.screwfix.ie/";
inline const std::string BV = "https://apps.bazaarvoice.com/bfd/v1/clients/screwfix-emea/api-products/cv2/resources/data/reviews.json";

inline Delivery siteDelivery(double amount, std::optional<bool> canDeliver) {
    if (canDeliver.has_value() && !*canDeliver) {
        return Delivery{.status = "unavailable", .source = "listing"};
    }
    return Delivery{
        .status = canDeliver.value_or(false) ? "available" : "unknown",
        .minDays = 2,
        .maxDays = 3,
        .cost = (amount >= 50.0) ? 0.0 : 5.0,
        .source = canDeliver.has_value() ? "listing" : "site default"
    };
}

inline std::string collectLabel(std::string_view status) {
    std::string s(status);
    for (char& c : s) {
        if (c == '_') c = ' ';
        else c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (!s.empty()) {
        s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    }
    return s;
}

inline std::runtime_error noDataError(std::string_view html, std::string_view componentName) {
    if (html.find("__NEXT_DATA__") == std::string_view::npos) {
        static const std::regex titleRe(R"(<title>([^<]{0,80}))", std::regex::icase);
        std::string htmlStr(html);
        std::smatch m;
        std::string title = "no title";
        if (std::regex_search(htmlStr, m, titleRe)) {
            title = text(m[1].str());
            if (title.empty()) title = "no title";
        }
        return std::runtime_error(
            "screwfix: the page has no data (a bot check or error page, not a layout change); tell the user and don't retry: " + title);
    }
    return std::runtime_error("screwfix: no " + std::string(componentName) + " data on the page (layout changed?)");
}

inline std::optional<slimr::Json> component(std::string_view html, std::string_view name) {
    static const std::regex scriptRe(R"(<script id="__NEXT_DATA__"[^>]*>([\s\S]*?)</script>)");
    std::string htmlStr(html);
    std::smatch m;
    if (!std::regex_search(htmlStr, m, scriptRe)) {
        return std::nullopt;
    }
    std::string jsonStr = m[1].str();
    auto parsed = slimr::parse_json<slimr::Json>(jsonStr);
    if (!parsed || !parsed->is_object()) return std::nullopt;

    auto& root = *parsed;
    if (!root.contains("props") || !root["props"].is_object()) return std::nullopt;
    auto& props = root["props"];
    if (!props.contains("pageProps") || !props["pageProps"].is_object()) return std::nullopt;
    auto& pageProps = props["pageProps"];
    if (!pageProps.contains("page") || !pageProps["page"].is_object()) return std::nullopt;
    auto& pageObj = pageProps["page"];
    if (!pageObj.contains("page") || !pageObj["page"].is_object()) return std::nullopt;
    auto& componentsObj = pageObj["page"];

    for (const auto& [_, compVal] : componentsObj.get<slimr::Json::object_t>()) {
        if (!compVal.is_object()) continue;
        if (compVal.contains("name") && compVal["name"].is_string() && compVal["name"].get<std::string>() == name) {
            if (compVal.contains("models") && compVal["models"].is_object()) {
                auto& models = compVal["models"];
                if (models.contains("enrichedData")) {
                    return models["enrichedData"];
                }
            }
        }
    }
    return std::nullopt;
}

inline std::optional<Offer> toOffer(const slimr::Json& row, std::string_view checkedAt, bool listing, std::string_view store) {
    if (!row.is_object()) return std::nullopt;
    std::string skuId;
    if (row.contains("skuId") && row["skuId"].is_string()) {
        skuId = row["skuId"].get<std::string>();
    }
    if (skuId.empty()) return std::nullopt;

    std::optional<double> amount;
    std::optional<double> was;
    std::string currency = "EUR";
    std::optional<double> priceExVat;

    if (row.contains("priceInformation") && row["priceInformation"].is_object()) {
        auto& priceInfo = row["priceInformation"];
        if (priceInfo.contains("currentPriceIncVat") && priceInfo["currentPriceIncVat"].is_object()) {
            auto& incVat = priceInfo["currentPriceIncVat"];
            if (incVat.contains("amount") && incVat["amount"].is_number()) {
                amount = incVat["amount"].get<double>();
            }
            if (incVat.contains("currency") && incVat["currency"].is_string()) {
                currency = incVat["currency"].get<std::string>();
            }
        }
        if (priceInfo.contains("wasPriceIncVat") && priceInfo["wasPriceIncVat"].is_object()) {
            auto& wasObj = priceInfo["wasPriceIncVat"];
            if (wasObj.contains("amount") && wasObj["amount"].is_number()) {
                was = wasObj["amount"].get<double>();
            }
        }
        if (priceInfo.contains("currentPriceExVat") && priceInfo["currentPriceExVat"].is_object()) {
            auto& exVatObj = priceInfo["currentPriceExVat"];
            if (exVatObj.contains("amount") && exVatObj["amount"].is_number()) {
                priceExVat = exVatObj["amount"].get<double>();
            }
        }
    }
    if (!amount.has_value()) return std::nullopt;

    std::optional<bool> canDeliver;
    std::optional<bool> canCollect;
    if (row.contains("fulfilmentAvailability") && row["fulfilmentAvailability"].is_object()) {
        auto& fa = row["fulfilmentAvailability"];
        if (fa.contains("canBeDelivered") && fa["canBeDelivered"].is_boolean()) {
            canDeliver = fa["canBeDelivered"].get<bool>();
        }
        if (fa.contains("canBeCollected") && fa["canBeCollected"].is_boolean()) {
            canCollect = fa["canBeCollected"].get<bool>();
        }
    }

    bool collectionOnly = false;
    if (row.contains("collectionOnly") && row["collectionOnly"].is_boolean()) {
        collectionOnly = row["collectionOnly"].get<bool>();
    }

    std::map<std::string, slimr::Json> details;
    if (row.contains("bullets") && row["bullets"].is_array() && !row["bullets"].get<slimr::Json::array_t>().empty()) {
        details["Features"] = row["bullets"];
    }

    std::string statusStr;
    if (row.contains("productAvailabilityStatus") && row["productAvailabilityStatus"].is_string()) {
        statusStr = row["productAvailabilityStatus"].get<std::string>();
    }

    if (!store.empty() && !statusStr.empty()) {
        details["Stock at " + std::string(store)] = collectLabel(statusStr);
    } else if (canCollect.has_value()) {
        details["Click & Collect"] = *canCollect ? "Yes" : "No";
    }

    if (priceExVat.has_value()) {
        details["Price ex VAT"] = *priceExVat;
    }

    std::optional<std::string> model;
    if (!listing) {
        if (row.contains("technicalSpecifications") && row["technicalSpecifications"].is_array()) {
            for (const auto& s : row["technicalSpecifications"].get<slimr::Json::array_t>()) {
                if (!s.is_object()) continue;
                std::string k = s.contains("name") && s["name"].is_string() ? text(s["name"].get<std::string>()) : "";
                std::string v = s.contains("value") && s["value"].is_string() ? text(s["value"].get<std::string>()) : "";
                if (!k.empty() && !v.empty()) {
                    details[k] = v;
                    if (k == "Model No") {
                        model = v;
                    }
                }
            }
        }
        if (row.contains("overview") && row["overview"].is_string()) {
            std::string ov = text(row["overview"].get<std::string>());
            if (!ov.empty()) {
                details["Overview"] = ov;
            }
        }
    }

    std::string detailPageUrl;
    if (row.contains("detailPageUrl") && row["detailPageUrl"].is_string()) {
        detailPageUrl = row["detailPageUrl"].get<std::string>();
    }
    std::string skuLower = skuId;
    for (char& c : skuLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    std::string url = !detailPageUrl.empty() ? canonicalUrl(detailPageUrl, BASE) : BASE + "p/item/" + skuLower;

    std::string skuUpper = skuId;
    for (char& c : skuUpper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    std::string name = row.contains("longDescription") && row["longDescription"].is_string() ? text(row["longDescription"].get<std::string>()) : "";

    std::optional<std::string> brand;
    if (row.contains("brand") && row["brand"].is_string()) {
        std::string b = text(row["brand"].get<std::string>());
        if (!b.empty()) brand = std::move(b);
    }

    std::optional<std::string> category;
    if (row.contains("category") && row["category"].is_object() && row["category"].contains("name") && row["category"]["name"].is_string()) {
        category = text(row["category"]["name"].get<std::string>());
    } else if (row.contains("defaultParentCategory") && row["defaultParentCategory"].is_object() && row["defaultParentCategory"].contains("name") && row["defaultParentCategory"]["name"].is_string()) {
        category = text(row["defaultParentCategory"]["name"].get<std::string>());
    }
    if (category && category->empty()) category = std::nullopt;

    std::optional<Rating> rating;
    if (row.contains("starRating") && row["starRating"].is_number() && row.contains("numberOfReviews") && row["numberOfReviews"].is_number()) {
        double rVal = row["starRating"].get<double>();
        int rCount = static_cast<int>(row["numberOfReviews"].get<double>());
        if (rCount > 0) {
            rating = Rating{.value = rVal, .scale = 5.0, .count = rCount};
        }
    }

    std::optional<bool> inStock;
    if (canDeliver.has_value() || canCollect.has_value()) {
        inStock = (canDeliver.value_or(false) || canCollect.value_or(false));
    }

    Offer o;
    o.site = "screwfix";
    o.id = std::move(skuUpper);
    o.url = std::move(url);
    o.name = std::move(name);
    o.brand = std::move(brand);
    o.model = std::move(model);
    o.category = std::move(category);
    o.price = Price{
        .amount = *amount,
        .currency = std::move(currency),
        .was = (was.has_value() && *was > *amount) ? was : std::nullopt,
        .taxIncluded = true
    };
    o.rating = std::move(rating);
    o.inStock = inStock;
    o.delivery = siteDelivery(*amount, collectionOnly ? std::optional<bool>(false) : canDeliver);
    o.details = std::move(details);
    o.checkedAt = std::string(checkedAt);
    return o;
}

} // namespace screwfix_internal

inline SearchResult parseScrewfixSearch(std::string_view html, std::string_view checkedAt) {
    using namespace screwfix_internal;
    auto listingOpt = component(html, "search-results");
    if (!listingOpt) {
        listingOpt = component(html, "product-listing");
    }
    if (!listingOpt) {
        throw noDataError(html, "search-results");
    }

    auto& listing = *listingOpt;
    std::string store;
    if (listing.is_object() && listing.contains("store") && listing["store"].is_object() &&
        listing["store"].contains("name") && listing["store"]["name"].is_string()) {
        store = text(listing["store"]["name"].get<std::string>());
    }

    std::vector<Offer> offers;
    if (listing.is_object() && listing.contains("products") && listing["products"].is_array()) {
        for (const auto& row : listing["products"].get<slimr::Json::array_t>()) {
            auto o = toOffer(row, checkedAt, true, store);
            if (o) offers.push_back(std::move(*o));
        }
    }

    std::optional<int> total;
    if (listing.is_object() && listing.contains("totalProducts") && listing["totalProducts"].is_number()) {
        total = static_cast<int>(listing["totalProducts"].get<double>());
    }

    return SearchResult{.offers = std::move(offers), .total = total};
}

inline Offer parseScrewfixProduct(std::string_view html, std::string_view checkedAt) {
    using namespace screwfix_internal;
    auto detailsOpt = component(html, "product-details");
    auto headerOpt = component(html, "header");

    std::string store;
    if (headerOpt && headerOpt->is_object() && headerOpt->contains("store") && (*headerOpt)["store"].is_object() &&
        (*headerOpt)["store"].contains("name") && (*headerOpt)["store"]["name"].is_string()) {
        store = text((*headerOpt)["store"]["name"].get<std::string>());
    }

    if (!detailsOpt || !detailsOpt->is_object() || !detailsOpt->contains("productDetails")) {
        throw noDataError(html, "product-details");
    }

    auto row = (*detailsOpt)["productDetails"];
    if (row.is_object() && (!row.contains("detailPageUrl") || !row["detailPageUrl"].is_string() || row["detailPageUrl"].get<std::string>().empty())) {
        static const std::regex canonRe(R"re(<link rel="canonical" href="([^"]+)")re");
        std::string htmlStr(html);
        std::smatch m;
        if (std::regex_search(htmlStr, m, canonRe)) {
            row["detailPageUrl"] = m[1].str();
        }
    }

    auto o = toOffer(row, checkedAt, false, store);
    if (!o) {
        throw noDataError(html, "product-details");
    }
    return *o;
}

inline ReviewsResult parseScrewfixReviews(const slimr::Json& json) {
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

inline std::vector<Pickup> parseScrewfixStock(const slimr::Json& json, std::string_view skuId, double radiusKm) {
    std::string skuUpper(skuId);
    for (char& c : skuUpper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    std::vector<Pickup> rows;
    if (!json.is_object() || !json.contains("nearestStores") || !json["nearestStores"].is_array()) {
        return rows;
    }

    for (const auto& b : json["nearestStores"].get<slimr::Json::array_t>()) {
        if (!b.is_object() || !b.contains("storeInfo") || !b["storeInfo"].is_object()) continue;
        auto& storeInfo = b["storeInfo"];
        double directDistance = -1.0;
        if (storeInfo.contains("directDistance") && storeInfo["directDistance"].is_number()) {
            directDistance = storeInfo["directDistance"].get<double>();
        }
        if (directDistance < 0) continue;
        double km = std::round((directDistance / 100.0)) / 10.0;
        if (km > radiusKm) continue;

        if (!storeInfo.contains("store") || !storeInfo["store"].is_object()) continue;
        auto& store = storeInfo["store"];
        if (store.contains("isClosed") && store["isClosed"].is_boolean() && store["isClosed"].get<bool>()) {
            continue;
        }

        std::string name;
        if (store.contains("basic") && store["basic"].is_object() && store["basic"].contains("name") && store["basic"]["name"].is_string()) {
            name = text(store["basic"]["name"].get<std::string>());
        }
        if (name.empty()) continue;

        std::optional<int> qty;
        std::optional<std::string> readyBy;
        if (b.contains("products") && b["products"].is_array()) {
            for (const auto& p : b["products"].get<slimr::Json::array_t>()) {
                if (!p.is_object()) continue;
                std::string pSku;
                if (p.contains("skuId") && p["skuId"].is_string()) {
                    pSku = p["skuId"].get<std::string>();
                    for (char& c : pSku) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                }
                if (pSku == skuUpper) {
                    if (p.contains("branchStock") && p["branchStock"].is_number()) {
                        qty = static_cast<int>(p["branchStock"].get<double>());
                    }
                    std::string availStatus;
                    if (p.contains("productAvailabilityStatus") && p["productAvailabilityStatus"].is_string()) {
                        availStatus = p["productAvailabilityStatus"].get<std::string>();
                    }
                    if (availStatus.find("TODAY") != std::string::npos) {
                        readyBy = "today";
                    } else if (availStatus.find("NEXT_DAY") != std::string::npos) {
                        readyBy = "next day";
                    } else if (p.contains("collectionPromiseDate") && p["collectionPromiseDate"].is_string()) {
                        readyBy = p["collectionPromiseDate"].get<std::string>();
                    }
                    break;
                }
            }
        }

        std::string status = "unknown";
        if (qty.has_value()) {
            status = (*qty <= 0) ? "out" : (*qty <= 2) ? "low" : "in stock";
        }

        rows.push_back(Pickup{
            .store = std::move(name),
            .km = km,
            .status = std::move(status),
            .qty = qty,
            .readyBy = std::move(readyBy)
        });
    }

    std::sort(rows.begin(), rows.end(), [](const Pickup& a, const Pickup& b) {
        return a.km < b.km;
    });
    return rows;
}

class ScrewfixAdapter : public SiteAdapter {
public:
    std::string id() const override { return "screwfix"; }
    std::string matchHint() const override {
        return "Screwfix matches loosely; try the product type or the trade term (\"combi drill\", \"wood screws\").";
    }

    SearchResult search(SiteContext& ctx, const SearchQuery& query) override {
        std::string enc = slimr::url_encode(query.query);
        for (size_t pos = 0; (pos = enc.find("%20", pos)) != std::string::npos; ) {
            enc.replace(pos, 3, "+");
            pos += 1;
        }
        std::string url = screwfix_internal::BASE + "search?search=" + enc;
        std::string html = ctx.fetchText(url);
        return parseScrewfixSearch(html, "");
    }

    std::vector<Offer> offer(SiteContext& ctx, const OfferRef& ref, const OfferOptions& options) override {
        (void)options;
        std::string url = ref.url.value_or("");
        if (url.empty()) {
            std::string idLower = ref.id.value_or("");
            for (char& c : idLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            url = screwfix_internal::BASE + "p/item/" + idLower;
        }
        std::string html = ctx.fetchText(canonicalUrl(url, screwfix_internal::BASE));
        return {parseScrewfixProduct(html, "")};
    }

    ReviewsResult reviews(SiteContext& ctx, const OfferRef& ref, const ReviewQuery& query) override {
        std::string id = ref.id.value_or("");
        if (id.empty() && ref.url) {
            static const std::regex idRe(R"(/([0-9a-zA-Z]+)$)");
            std::smatch m;
            if (std::regex_search(*ref.url, m, idRe)) {
                id = m[1].str();
            }
        }
        if (id.empty()) {
            throw std::runtime_error("screwfix: give the product id");
        }
        std::string idUpper = id;
        for (char& c : idUpper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

        std::string sortParam = "rating:asc";
        if (query.sort == "newest") sortParam = "submissiontime:desc";
        else if (query.sort == "helpful") sortParam = "helpfulness:desc";

        int limit = std::min(query.limit, 100);
        std::string url = screwfix_internal::BV +
            "?resource=reviews&action=REVIEWS_N_STATS&filteredstats=reviews&Stats=Reviews"
            "&limit=" + std::to_string(limit) +
            "&offset=0&sort=" + slimr::url_encode(sortParam) +
            "&apiversion=5.5&displaycode=22465-en_ie"
            "&filter=productid%3Aeq%3A" + slimr::url_encode(idUpper) +
            "&filter=contentlocale%3Aeq%3Aen_GB%2Cen_IE"
            "&filter=isratingsonly%3Aeq%3Afalse";

        FetchInit init;
        init.headers["bv-bfd-token"] = "22465,main_site,en_IE";
        slimr::Json json = ctx.fetchJson(url, init);
        return parseScrewfixReviews(json);
    }

    std::vector<Offer> pickup(SiteContext& ctx, const std::vector<Offer>& offers, const Near& near) override {
        std::string script = R"js((() => {
            const m = /"tykToken":"([^"]+)"/.exec(document.getElementById("__NEXT_DATA__")?.textContent ?? "");
            return m ? m[1] : "";
        })())js";
        auto tokenJson = ctx.evaluate(script);
        std::string token;
        if (tokenJson.is_string()) token = tokenJson.get<std::string>();
        if (token.empty()) {
            throw std::runtime_error("screwfix: no stock API token in the page config (layout changed?)");
        }

        std::vector<Offer> out;
        out.reserve(offers.size());
        for (auto offer : offers) {
            std::string url = screwfix_internal::BASE + "prod/ffx-browse-bff/v1/SFXIE/stock/search?productId=" +
                slimr::url_encode(offer.id) +
                "&lat=" + std::to_string(near.lat) +
                "&long=" + std::to_string(near.lon) +
                "&quantity=1";

            FetchInit init;
            init.headers["authorization"] = token;
            init.headers["content-type"] = "application/json";
            init.referrer = screwfix_internal::BASE + "checkstock?product_id=" + slimr::url_encode(offer.id) + "&quantity=1";

            slimr::Json json = ctx.fetchJson(url, init);
            offer.pickup = parseScrewfixStock(json, offer.id, near.radiusKm);
            out.push_back(std::move(offer));
        }
        return out;
    }
};

} // namespace shop::sites
