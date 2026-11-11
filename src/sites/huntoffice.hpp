#pragma once

#include "site_helpers.hpp"

#include <format>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace shop::sites::huntoffice_ns {

inline const std::set<std::string> HUNTOFFICE_SKIP{
    "id", "thumbnail", "url", "name", "longDescription", "sku", "reference", "keywords",
    "category_level_1", "category_level_2", "manufacturer", "price", "originalPrice", "campaign",
    "inStock", "delivery", "vatPrice", "values", "variants", "boost", "statusCode", "sales",
    "detailed_thumb_url", "enable_2nd_thumbnail", "prices_from"
};

inline std::string huntofficePageUrl(std::string_view href) {
    const std::string BASE = "https://www.huntoffice.ie/";
    std::string u = core::canonicalUrl(href, BASE);
    if (u.ends_with(".html")) {
        u.resize(u.size() - 5);
    }
    return u;
}

inline core::Delivery parseHuntOfficeDelivery(std::string_view label, std::optional<bool> inStock) {
    if (inStock.has_value() && !*inStock) {
        return core::Delivery{.status = "unavailable", .source = "listing"};
    }
    std::string s(label);
    static const std::regex daysRe(R"((\d+)\s*-\s*(\d+)\s*days)", std::regex::icase);
    std::smatch m;
    if (std::regex_search(s, m, daysRe)) {
        return core::Delivery{
            .status = "available",
            .minDays = std::stoi(m[1].str()),
            .maxDays = std::stoi(m[2].str()),
            .cost = 9.99,
            .source = "listing"
        };
    }
    return core::Delivery{
        .status = "available",
        .cost = 9.99,
        .source = "site default"
    };
}

inline std::optional<core::Offer> huntofficeFromItem(const slimr::Json& it, const std::string& checkedAt) {
    if (!it.is_object() || !it.contains("attributes") || !it["attributes"].is_array()) {
        return std::nullopt;
    }

    auto getAttrValues = [&](std::string_view name) -> const slimr::Json::array_t* {
        for (const auto& a : it["attributes"].get<slimr::Json::array_t>()) {
            if (a.is_object() && a.contains("name") && a["name"].is_string() && a["name"].get<std::string>() == name) {
                if (a.contains("values") && a["values"].is_array()) {
                    return &a["values"].get<slimr::Json::array_t>();
                }
            }
        }
        return nullptr;
    };

    auto getOneStr = [&](std::string_view name) -> std::string {
        auto* vals = getAttrValues(name);
        if (vals && !vals->empty()) {
            const auto& first = (*vals)[0];
            if (first.is_string()) return first.get<std::string>();
            if (first.is_number()) return std::to_string(first.as<double>());
            if (first.is_boolean()) return first.as<bool>() ? "true" : "false";
        }
        return "";
    };

    std::string rawName = getOneStr("name");
    std::string name = cleanText(decodeHtmlEntities(rawName));
    std::string url = getOneStr("url");

    std::optional<double> inc;
    std::string vatPriceStr = getOneStr("vatPrice");
    if (!vatPriceStr.empty()) inc = core::parsePrice(vatPriceStr);

    std::optional<double> ex;
    std::string priceStr = getOneStr("price");
    if (!priceStr.empty()) ex = core::parsePrice(priceStr);

    std::optional<double> amount = inc.has_value() ? inc : ex;
    if (name.empty() || url.empty() || !amount || *amount <= 0.0) {
        return std::nullopt;
    }

    std::string brand = getOneStr("manufacturer");
    if (brand.empty()) brand = getOneStr("Brand");
    brand = cleanText(decodeHtmlEntities(brand));

    std::string category = getOneStr("category_level_2");
    if (category.empty()) category = getOneStr("category_level_1");
    category = cleanText(decodeHtmlEntities(category));

    std::optional<bool> inStock;
    auto* inStockVals = getAttrValues("inStock");
    if (inStockVals && !inStockVals->empty()) {
        const auto& first = (*inStockVals)[0];
        if (first.is_boolean()) inStock = first.as<bool>();
    }

    auto quantity = core::parseQuantity(name);

    std::map<std::string, slimr::Json> details;
    for (const auto& a : it["attributes"].get<slimr::Json::array_t>()) {
        if (!a.is_object() || !a.contains("name") || !a["name"].is_string()) continue;
        std::string aName = a["name"].get<std::string>();
        if (HUNTOFFICE_SKIP.contains(aName) || aName.ends_with("_pos")) continue;
        if (!a.contains("values") || !a["values"].is_array()) continue;

        std::string joined;
        for (const auto& v : a["values"].get<slimr::Json::array_t>()) {
            std::string s;
            if (v.is_string()) s = v.get<std::string>();
            else if (v.is_number()) s = std::to_string(v.as<double>());
            else if (v.is_boolean()) s = v.as<bool>() ? "true" : "false";
            s = cleanText(decodeHtmlEntities(s));
            if (!s.empty()) {
                if (!joined.empty()) joined += ", ";
                joined += s;
            }
        }
        if (!joined.empty()) {
            details[aName] = slimr::Json(joined);
        }
    }

    if (ex.has_value()) {
        details["Price ex VAT"] = slimr::Json(formatEuro2(*ex));
    }

    std::string ref = getOneStr("reference");
    if (!ref.empty()) {
        details["Model"] = slimr::Json(cleanText(ref));
    }

    std::string id = it.contains("id")
                         ? (it["id"].is_string() ? it["id"].get<std::string>() : std::to_string(it["id"].as<int64_t>()))
                         : "";

    std::string deliveryStr = getOneStr("delivery");
    auto delivery = parseHuntOfficeDelivery(deliveryStr, inStock);

    core::Offer offer;
    offer.site = "huntoffice";
    offer.id = id;
    offer.url = huntofficePageUrl(url);
    offer.name = name;
    if (!brand.empty()) offer.brand = brand;
    if (!category.empty()) offer.category = category;
    offer.price = core::Price{
        .amount = *amount,
        .currency = "EUR",
        .taxIncluded = inc.has_value()
    };
    if (quantity) offer.quantity = *quantity;
    if (inStock.has_value()) offer.inStock = *inStock;
    offer.delivery = delivery;
    if (!details.empty()) offer.details = std::move(details);
    offer.checkedAt = checkedAt;

    return offer;
}

inline core::SearchResult parseSearch(const slimr::Json& json, const std::string& checkedAt) {
    std::vector<core::Offer> offers;
    std::optional<int> total;

    if (json.is_object() && json.contains("results") && json["results"].is_object()) {
        const auto& results = json["results"];
        if (results.contains("count") && results["count"].is_number()) {
            total = results["count"].as<int>();
        }
        if (results.contains("items") && results["items"].is_array()) {
            for (const auto& it : results["items"].get<slimr::Json::array_t>()) {
                auto o = huntofficeFromItem(it, checkedAt);
                if (o) {
                    offers.push_back(std::move(*o));
                }
            }
        }
    }

    return core::SearchResult{.offers = std::move(offers), .total = total};
}

inline core::Offer parseProduct(std::string_view html, const std::string& checkedAt, const std::optional<core::Offer>& row = std::nullopt) {
    auto doc = core::Document::parse(html);
    auto blocks = core::jsonLdBlocks(html);
    auto ld = core::findLd(blocks, "Product");

    std::string id;
    if (row) {
        id = row->id;
    } else if (ld && ld->is_object() && ld->contains("sku") && (*ld)["sku"].is_string()) {
        std::string sku = (*ld)["sku"].get<std::string>();
        static const std::regex hRe(R"(^H(?=\d))");
        id = std::regex_replace(sku, hRe, "");
    }

    std::string name;
    if (ld && ld->is_object() && ld->contains("name") && (*ld)["name"].is_string()) {
        name = cleanText((*ld)["name"].get<std::string>());
    } else {
        auto h1 = doc.querySelector("h1");
        if (h1) name = cleanText(h1->text());
        else if (row) name = row->name;
    }

    std::optional<double> inc;
    auto incEl = doc.querySelector(".inc-vat");
    if (incEl) inc = core::parsePrice(incEl->text());
    if (!inc && ld && ld->is_object() && ld->contains("offers")) {
        const auto& offersVal = (*ld)["offers"];
        if (offersVal.is_array() && !offersVal.get<slimr::Json::array_t>().empty()) {
            const auto& first = offersVal.get<slimr::Json::array_t>()[0];
            if (first.is_object() && first.contains("price")) {
                if (first["price"].is_string()) inc = core::parsePrice(first["price"].get<std::string>());
                else if (first["price"].is_number()) inc = first["price"].as<double>();
            }
        }
    }

    std::optional<double> ex;
    auto exEl = doc.querySelector(".ex-vat");
    if (exEl) ex = core::parsePrice(exEl->text());

    std::string canonical;
    auto canEl = doc.querySelector("link[rel=\"canonical\"]");
    if (canEl) {
        auto href = canEl->attr("href");
        if (href) canonical = *href;
    }

    if (id.empty() || name.empty() || (!inc && !ex)) {
        throw std::runtime_error("huntoffice: no product data on the page (layout changed?)");
    }

    std::string availability;
    if (ld && ld->is_object() && ld->contains("offers")) {
        const auto& offersVal = (*ld)["offers"];
        if (offersVal.is_array() && !offersVal.get<slimr::Json::array_t>().empty()) {
            const auto& first = offersVal.get<slimr::Json::array_t>()[0];
            if (first.is_object() && first.contains("availability") && first["availability"].is_string()) {
                availability = first["availability"].get<std::string>();
            }
        }
    }

    std::optional<bool> inStock;
    static const std::regex outRe(R"(OutOfStock|SoldOut)", std::regex::icase);
    static const std::regex inRe(R"(InStock)", std::regex::icase);
    if (std::regex_search(availability, outRe)) inStock = false;
    else if (std::regex_search(availability, inRe)) inStock = true;
    else if (row) inStock = row->inStock;

    std::string timelineText;
    auto timeEl = doc.querySelector(".timeline-text");
    if (timeEl) timelineText = cleanText(timeEl->text());
    auto delivery = parseHuntOfficeDelivery(timelineText, inStock);

    std::optional<std::string> brand;
    if (ld && ld->is_object() && ld->contains("brand") && (*ld)["brand"].is_object() &&
        (*ld)["brand"].contains("name") && (*ld)["brand"]["name"].is_string()) {
        brand = cleanText((*ld)["brand"]["name"].get<std::string>());
    } else if (row && row->brand) {
        brand = row->brand;
    }

    auto quantity = core::parseQuantity(name);

    std::map<std::string, slimr::Json> details;
    if (row) {
        details = row->details;
    }
    if (ex.has_value()) {
        details["Price ex VAT"] = slimr::Json(formatEuro2(*ex));
    }
    if (ld && ld->is_object() && ld->contains("mpn") && (*ld)["mpn"].is_string()) {
        details["Model"] = slimr::Json(cleanText((*ld)["mpn"].get<std::string>()));
    }

    const std::string BASE = "https://www.huntoffice.ie/";
    std::string url = !canonical.empty() ? huntofficePageUrl(canonical) : (row ? row->url : BASE);

    core::Offer offer;
    offer.site = "huntoffice";
    offer.id = id;
    offer.url = url;
    offer.name = name;
    if (brand) offer.brand = *brand;
    if (row && row->category) offer.category = row->category;
    if (inc.has_value()) {
        offer.price = core::Price{.amount = *inc, .currency = "EUR", .taxIncluded = true};
    } else {
        offer.price = core::Price{.amount = *ex, .currency = "EUR", .taxIncluded = false};
    }
    if (quantity) offer.quantity = *quantity;
    if (inStock.has_value()) offer.inStock = *inStock;
    offer.delivery = delivery;
    offer.details = std::move(details);
    offer.checkedAt = checkedAt;

    return offer;
}

class HuntOffice : public core::SiteAdapter {
public:
    std::string id() const override { return "huntoffice"; }
    bool deliveryOnly() const override { return true; }
    std::string matchHint() const override {
        return "Loop54 matches product words (\"mesh chair\", \"stapler\", \"a4 paper 80gsm\"). Bare category words (\"chair\", \"desk\", \"printer\") redirect to a category page and return no rows; add a qualifier or use a model code.";
    }

    core::SearchResult search(core::SiteContext& ctx, const core::SearchQuery& q) override {
        const std::string SEARCH = "https://huntoffice-ie.54proxy.com/search";
        int take = std::min(q.limit * 2, 60);

        slimr::Json body;
        body["query"] = q.query;
        slimr::Json::object_t ro;
        ro["facets"] = slimr::Json::array_t{};
        ro["skip"] = static_cast<int64_t>(0);
        ro["take"] = static_cast<int64_t>(take);
        slimr::Json::array_t sortBy;
        slimr::Json::object_t sortItem;
        sortItem["type"] = "relevance";
        sortItem["order"] = "desc";
        sortBy.push_back(std::move(sortItem));
        ro["sortBy"] = std::move(sortBy);
        body["resultsOptions"] = std::move(ro);

        core::FetchInit init;
        init.method = "POST";
        init.headers["api-version"] = "V3";
        init.headers["user-id"] = "shop-assistant";
        init.headers["Content-Type"] = "application/json";
        init.body = slimr::write_json(body);

        auto json = ctx.fetchJson(SEARCH, init);
        auto res = parseSearch(json, "2026-11-10T00:00:00Z");
        if (res.offers.empty() && json.is_object() && json.contains("redirect") && json["redirect"].is_string()) {
            ctx.warn("HuntOffice treats \"" + q.query + "\" as the category " + json["redirect"].get<std::string>() +
                     "; add a qualifier (e.g. \"mesh chair\") or a model code.");
        }
        return res;
    }

    std::vector<core::Offer> offer(core::SiteContext& ctx, const core::OfferRef& ref, const core::OfferOptions& options) override {
        (void)options;
        const std::string SEARCH = "https://huntoffice-ie.54proxy.com/search";
        std::optional<core::Offer> row;
        std::string url;
        if (ref.url) {
            url = huntofficePageUrl(*ref.url);
        } else {
            if (!ref.id) throw std::runtime_error("huntoffice: give an id or url");
            std::string id = *ref.id;
            slimr::Json body;
            body["query"] = "H" + id;
            slimr::Json::object_t ro;
            ro["facets"] = slimr::Json::array_t{};
            ro["skip"] = static_cast<int64_t>(0);
            ro["take"] = static_cast<int64_t>(5);
            body["resultsOptions"] = std::move(ro);

            core::FetchInit init;
            init.method = "POST";
            init.headers["api-version"] = "V3";
            init.headers["user-id"] = "shop-assistant";
            init.headers["Content-Type"] = "application/json";
            init.body = slimr::write_json(body);

            auto json = ctx.fetchJson(SEARCH, init);
            auto found = parseSearch(json, "").offers;
            for (const auto& o : found) {
                if (o.id == id) {
                    row = o;
                    break;
                }
            }
            if (!row) throw std::runtime_error("huntoffice: no product with id " + id);
            url = row->url;
        }

        std::string html = ctx.fetchText(url);
        return {parseProduct(html, "2026-11-10T00:00:00Z", row)};
    }
};

} // namespace shop::sites::huntoffice_ns
namespace shop::sites {
using HuntOffice = huntoffice_ns::HuntOffice;
using HuntOfficeAdapter = huntoffice_ns::HuntOffice;
} // namespace shop::sites
