#pragma once

#include "site_helpers.hpp"

#include <algorithm>
#include <format>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

namespace shop::sites::mcmahons_ns {

inline core::Delivery mcmahonsDeliveryFor(std::optional<bool> inStock) {
    if (inStock.has_value() && !*inStock) {
        return core::Delivery{.status = "unavailable", .source = "product"};
    }
    return core::Delivery{
        .status = "available",
        .minDays = 1,
        .maxDays = 2,
        .cost = 15.0,
        .source = "site default"
    };
}

struct McMahonsPriceResult {
    core::Price price;
    std::optional<double> exVat;
};

inline std::optional<McMahonsPriceResult> mcmahonsPriceFrom(const core::Node& root) {
    auto n = [&](std::string_view type) -> std::optional<double> {
        std::string sel = std::string("span[data-price-type=\"") + std::string(type) + "\"]";
        auto span = root.querySelector(sel);
        if (!span) return std::nullopt;
        auto amt = span->attr("data-price-amount");
        if (!amt) return std::nullopt;
        try {
            double v = std::stod(*amt);
            if (std::isfinite(v) && v > 0.0) return round2(v);
        } catch (...) {}
        return std::nullopt;
    };

    auto finalPrice = n("finalPrice");
    if (!finalPrice) return std::nullopt;

    auto oldPrice = n("oldPrice");
    auto basePrice = n("basePrice");

    core::Price p;
    p.amount = *finalPrice;
    p.taxIncluded = true;
    p.currency = "EUR";
    if (oldPrice && *oldPrice > *finalPrice) {
        p.was = *oldPrice;
    }

    return McMahonsPriceResult{.price = p, .exVat = basePrice};
}

inline core::Offer parseProduct(std::string_view html, const std::string& checkedAt);

inline core::SearchResult parseSearch(std::string_view html, const std::string& checkedAt) {
    auto doc = core::Document::parse(html);
    const std::string BASE = "https://www.mcmahons.ie/";

    // Check if redirected to product page
    if (!doc.querySelector(".toolbar-amount")) {
        auto blocks = core::jsonLdBlocks(html);
        if (core::findLd(blocks, "Product")) {
            return core::SearchResult{
                .offers = {parseProduct(html, checkedAt)},
                .total = 1
            };
        }
    }

    std::vector<core::Offer> offers;
    auto cards = doc.querySelectorAll("li.product-item");
    for (const auto& card : cards) {
        auto a = card.querySelector("a.product-name");
        if (!a) continue;
        auto hrefOpt = a->attr("href");
        if (!hrefOpt) continue;
        std::string href = *hrefOpt;
        std::string name = cleanText(a->text());

        static const std::regex skuRe(R"(-(\d+)/?$)");
        std::smatch m;
        if (!std::regex_search(href, m, skuRe)) continue;
        std::string sku = m[1].str();

        auto priceRes = mcmahonsPriceFrom(card);
        if (!priceRes || name.empty() || sku.empty()) continue;

        std::map<std::string, slimr::Json> details;
        if (priceRes->exVat.has_value()) {
            details["Price ex VAT"] = slimr::Json(*priceRes->exVat);
        }

        std::string id = sku;
        for (char& c : id) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

        core::Offer offer;
        offer.site = "mcmahons";
        offer.id = id;
        offer.url = core::canonicalUrl(href, BASE);
        offer.name = name;
        offer.price = priceRes->price;
        offer.delivery = mcmahonsDeliveryFor(std::nullopt);
        if (!details.empty()) offer.details = std::move(details);
        offer.checkedAt = checkedAt;

        offers.push_back(std::move(offer));
    }

    std::optional<int> total;
    auto toolbar = doc.querySelector(".toolbar-amount");
    if (toolbar) {
        std::string tbText = cleanText(toolbar->text());
        static const std::regex totalRe(R"(of\s+([\d,]+))", std::regex::icase);
        std::smatch m;
        if (std::regex_search(tbText, m, totalRe)) {
            std::string s = m[1].str();
            s.erase(std::remove(s.begin(), s.end(), ','), s.end());
            try {
                int t = std::stoi(s);
                if (t > 0) total = t;
            } catch (...) {}
        }
    }

    return core::SearchResult{.offers = std::move(offers), .total = total};
}

inline core::Offer parseProduct(std::string_view html, const std::string& checkedAt) {
    auto doc = core::Document::parse(html);
    const std::string BASE = "https://www.mcmahons.ie/";

    auto blocks = core::jsonLdBlocks(html);
    auto ld = core::findLd(blocks, "Product");

    std::string sku;
    if (ld && ld->is_object() && ld->contains("sku") && (*ld)["sku"].is_string()) {
        sku = cleanText((*ld)["sku"].get<std::string>());
    } else {
        auto skuEl = doc.querySelector(".product.attribute.sku [itemprop=\"sku\"]");
        if (skuEl) sku = cleanText(skuEl->text());
    }

    auto priceBox = doc.querySelector(".product-info-price");
    auto priceRes = priceBox ? mcmahonsPriceFrom(*priceBox) : mcmahonsPriceFrom(doc.rootNode());

    if (!ld || sku.empty() || !priceRes) {
        throw std::runtime_error("mcmahons: no product data on the page (layout changed?)");
    }

    std::map<std::string, std::string> specs;
    for (const auto& tr : doc.querySelectorAll("#product-attribute-specs-table tr")) {
        auto th = tr.querySelector("th");
        auto td = tr.querySelector("td");
        if (th && td) {
            std::string k = cleanText(th->text());
            std::string v = cleanText(td->text());
            if (!k.empty() && !v.empty()) {
                specs[k] = v;
            }
        }
    }

    std::string bodyText = doc.body().text();
    static const std::regex outBodyRe(R"(out of stock and not available)", std::regex::icase);
    bool outOfStock = std::regex_search(bodyText, outBodyRe);

    std::string availability;
    if (ld->contains("offers") && (*ld)["offers"].is_object() &&
        (*ld)["offers"].contains("availability") && (*ld)["offers"]["availability"].is_string()) {
        availability = (*ld)["offers"]["availability"].get<std::string>();
    }

    std::optional<bool> inStock;
    if (outOfStock || availability.find("OutOfStock") != std::string::npos) {
        inStock = false;
    } else if (availability.find("InStock") != std::string::npos) {
        inStock = true;
    }

    std::optional<std::string> brand;
    auto brandIt = specs.find("Brand");
    if (brandIt != specs.end()) {
        brand = brandIt->second;
        specs.erase(brandIt);
    }

    std::map<std::string, slimr::Json> details;
    if (priceRes->exVat.has_value()) {
        details["Price ex VAT"] = slimr::Json(*priceRes->exVat);
    }
    for (const auto& [k, v] : specs) {
        details[k] = slimr::Json(v);
    }

    if (ld->contains("description") && (*ld)["description"].is_string()) {
        std::string rawDesc = (*ld)["description"].get<std::string>();
        auto descDoc = core::Document::parse(std::string("<div>") + rawDesc + "</div>");
        std::string desc = cleanText(descDoc.body().text());
        if (!desc.empty()) {
            if (desc.size() > 600) desc = desc.substr(0, 600);
            details["Description"] = slimr::Json(desc);
        }
    }

    std::optional<std::string> category;
    if (ld->contains("category") && (*ld)["category"].is_string()) {
        category = cleanText((*ld)["category"].get<std::string>());
    }

    std::string url;
    if (ld->contains("url") && (*ld)["url"].is_string()) {
        url = (*ld)["url"].get<std::string>();
    } else {
        auto canEl = doc.querySelector("link[rel=\"canonical\"]");
        if (canEl) {
            auto href = canEl->attr("href");
            if (href) url = *href;
        }
    }

    std::string name = ld->contains("name") && (*ld)["name"].is_string()
                           ? cleanText((*ld)["name"].get<std::string>())
                           : "";

    std::string id = sku;
    for (char& c : id) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    core::Offer offer;
    offer.site = "mcmahons";
    offer.id = id;
    offer.url = core::canonicalUrl(url, BASE);
    offer.name = name;
    if (brand) offer.brand = *brand;
    if (category) offer.category = *category;
    offer.price = priceRes->price;
    if (inStock.has_value()) offer.inStock = *inStock;
    offer.delivery = mcmahonsDeliveryFor(inStock);
    offer.details = std::move(details);
    offer.checkedAt = checkedAt;

    return offer;
}

inline std::vector<core::Pickup> parseStock(std::string_view html, const core::Near& near) {
    const std::string tag = "<script type=\"text/x-magento-init\">";
    size_t at = html.find("\"kickModelSelCollectionBranch\"");
    if (at == std::string_view::npos) {
        throw std::runtime_error("mcmahons: no branch list on the page (layout changed?)");
    }

    size_t start = html.rfind(tag, at);
    if (start == std::string_view::npos) {
        throw std::runtime_error("mcmahons: no branch list script tag (layout changed?)");
    }
    start += tag.length();

    size_t end = html.find("</script>", start);
    if (end == std::string_view::npos) {
        throw std::runtime_error("mcmahons: unclosed script tag (layout changed?)");
    }

    std::string_view scriptContent = html.substr(start, end - start);
    auto parsed = slimr::parse_json<slimr::Json>(scriptContent);
    if (!parsed || !parsed->is_object()) {
        throw std::runtime_error("mcmahons: invalid branch config JSON");
    }

    std::optional<int64_t> productId;
    const slimr::Json::array_t* branchList = nullptr;

    try {
        if (parsed->contains("*") && (*parsed)["*"].is_object()) {
            const auto& star = (*parsed)["*"];
            if (star.contains("Magento_Ui/js/core/app") && star["Magento_Ui/js/core/app"].is_object()) {
                const auto& app = star["Magento_Ui/js/core/app"];
                if (app.contains("components") && app["components"].is_object()) {
                    const auto& comps = app["components"];
                    if (comps.contains("kickModelSelCollectionBranch") && comps["kickModelSelCollectionBranch"].is_object()) {
                        const auto& comp = comps["kickModelSelCollectionBranch"];
                        if (comp.contains("config") && comp["config"].is_object()) {
                            const auto& config = comp["config"];
                            if (config.contains("productId") && config["productId"].is_number()) {
                                productId = config["productId"].as<int64_t>();
                            }
                            if (config.contains("branchList") && config["branchList"].is_array()) {
                                branchList = &config["branchList"].get<slimr::Json::array_t>();
                            }
                        }
                    }
                }
            }
        }
    } catch (...) {}

    if (!branchList) {
        return {};
    }

    std::vector<core::Pickup> rows;
    for (const auto& b : *branchList) {
        if (!b.is_object()) continue;
        std::string store = b.contains("name") && b["name"].is_string() ? cleanText(b["name"].get<std::string>()) : "";
        if (store.empty()) continue;

        double lat = 0.0, lng = 0.0;
        try {
            if (b.contains("lat") && b["lat"].is_string()) lat = std::stod(b["lat"].get<std::string>());
            else if (b.contains("lat") && b["lat"].is_number()) lat = b["lat"].as<double>();
            if (b.contains("lng") && b["lng"].is_string()) lng = std::stod(b["lng"].get<std::string>());
            else if (b.contains("lng") && b["lng"].is_number()) lng = b["lng"].as<double>();
        } catch (...) {
            continue;
        }

        double km = std::round(core::distanceKm(near.lat, near.lon, lat, lng) * 10.0) / 10.0;
        if (!(km <= near.radiusKm)) continue;

        std::optional<int> qty;
        if (b.contains("stock") && b["stock"].is_object()) {
            const auto& stockObj = b["stock"].get<slimr::Json::object_t>();
            if (productId.has_value()) {
                std::string pidStr = std::to_string(*productId);
                auto it = stockObj.find(pidStr);
                if (it != stockObj.end() && it->second.is_number()) {
                    qty = it->second.as<int>();
                }
            }
            if (!qty.has_value() && !stockObj.empty()) {
                const auto& firstVal = stockObj.begin()->second;
                if (firstVal.is_number()) {
                    qty = firstVal.as<int>();
                }
            }
        }

        std::string status = "unknown";
        if (qty.has_value()) {
            if (*qty <= 0) status = "out";
            else if (*qty <= 2) status = "low";
            else status = "in stock";
        }

        core::Pickup p;
        p.store = store;
        p.km = km;
        p.status = status;
        if (qty.has_value()) p.qty = *qty;

        rows.push_back(std::move(p));
    }

    std::sort(rows.begin(), rows.end(), [](const core::Pickup& a, const core::Pickup& b) {
        return a.km < b.km;
    });

    return rows;
}

class McMahons : public core::SiteAdapter {
public:
    std::string id() const override { return "mcmahons"; }
    bool deliveryOnly() const override { return false; }
    std::string matchHint() const override {
        return "McMahons matches search words loosely and returns related items too; check the titles, and try the product type or the code alone.";
    }

    core::SearchResult search(core::SiteContext& ctx, const core::SearchQuery& q) override {
        std::string url = "https://www.mcmahons.ie/catalogsearch/result/?q=" + urlEncode(q.query, true);
        std::string html = ctx.fetchText(url);
        return parseSearch(html, "2026-11-10T00:00:00Z");
    }

    std::vector<core::Offer> offer(core::SiteContext& ctx, const core::OfferRef& ref, const core::OfferOptions& options) override {
        (void)options;
        const std::string BASE = "https://www.mcmahons.ie/";
        std::string url = ref.url.value_or("");
        if (url.empty()) {
            if (!ref.id) throw std::runtime_error("mcmahons: give a product id or url");
            std::string id = *ref.id;
            auto sr = search(ctx, core::SearchQuery{.query = id, .limit = 20});
            for (const auto& o : sr.offers) {
                if (toLower(o.id) == toLower(id)) {
                    url = o.url;
                    break;
                }
            }
            if (url.empty()) {
                throw std::runtime_error("mcmahons: no product with id " + id + "; search for it first");
            }
        }
        std::string html = ctx.fetchText(core::canonicalUrl(url, BASE));
        return {parseProduct(html, "2026-11-10T00:00:00Z")};
    }

    std::vector<core::Offer> pickup(core::SiteContext& ctx, const std::vector<core::Offer>& offers, const core::Near& near) override {
        const std::string BASE = "https://www.mcmahons.ie/";
        std::vector<core::Offer> out;
        for (auto o : offers) {
            std::string html = ctx.fetchText(core::canonicalUrl(o.url, BASE));
            o.pickup = parseStock(html, near);
            out.push_back(std::move(o));
        }
        return out;
    }
};

} // namespace shop::sites::mcmahons_ns
namespace shop::sites {
using McMahons = mcmahons_ns::McMahons;
using McMahonsAdapter = mcmahons_ns::McMahons;
} // namespace shop::sites
