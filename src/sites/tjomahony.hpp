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

namespace tjomahony_internal {

inline const std::string BASE = "https://tjomahony.ie/";
inline const Delivery DELIVERY_UNKNOWN{.status = "unknown", .source = "site default"};

inline Delivery deliveryFor(std::string_view rule, std::optional<bool> inStock) {
    if (inStock.has_value() && !*inStock) {
        return Delivery{.status = "unavailable", .source = "product"};
    }
    std::string r(rule);
    for (char& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (r.starts_with("small")) {
        return Delivery{.status = "available", .minDays = 3, .maxDays = 4, .cost = 9.99, .source = "site default"};
    }
    if (r.starts_with("large")) {
        return Delivery{.status = "available", .minDays = 5, .maxDays = 7, .cost = 29.99, .source = "site default"};
    }
    return DELIVERY_UNKNOWN;
}

struct ParsedPrice {
    Price price;
    std::optional<double> exVat;
};

inline std::optional<ParsedPrice> priceFrom(const Node& root) {
    if (!root) return std::nullopt;
    auto getAmount = [&](std::string_view type) -> std::optional<double> {
        std::string sel = "span[data-price-type=\"" + std::string(type) + "\"]";
        auto span = root.querySelector(sel);
        if (!span) return std::nullopt;
        auto attr = span->attr("data-price-amount");
        if (!attr) return std::nullopt;
        try {
            double v = std::stod(*attr);
            if (std::isfinite(v) && v > 0) return v;
        } catch (...) {}
        return std::nullopt;
    };

    auto amount = getAmount("finalPrice");
    if (!amount) return std::nullopt;
    auto was = getAmount("oldPrice");
    auto exVat = getAmount("basePrice");

    ParsedPrice res;
    res.price.amount = *amount;
    res.price.currency = "EUR";
    res.price.taxIncluded = true;
    if (was && *was > *amount) {
        res.price.was = *was;
    }
    if (exVat) {
        res.exVat = std::round(*exVat * 100.0) / 100.0;
    }
    return res;
}

} // namespace tjomahony_internal

inline Offer parseTjomahonyProduct(std::string_view html, std::string_view checkedAt);

inline SearchResult parseTjomahonySearch(std::string_view html, std::string_view checkedAt) {
    using namespace tjomahony_internal;
    auto doc = Document::parse(html);

    auto items = doc.querySelectorAll("li.product-item");
    if (items.empty()) {
        auto blocks = doc.jsonLd();
        if (findLd(blocks, "Product")) {
            return SearchResult{.offers = {parseTjomahonyProduct(html, checkedAt)}, .total = 1};
        }
    }

    std::vector<Offer> offers;
    for (const auto& card : items) {
        auto marker = card.querySelector("a.marker");
        if (!marker) continue;
        auto href = marker->attr("href");
        if (!href) continue;

        auto nameEl = card.querySelector(".product-name");
        std::string name = nameEl ? text(nameEl->text()) : "";

        auto skuEl = card.querySelector("[data-mage-init*=\"productSku\"]");
        std::string init = skuEl ? skuEl->attr("data-mage-init").value_or("") : "";
        static const std::regex skuRe(R"re("productSku":"([^"]+)")re");
        std::smatch m;
        if (!std::regex_search(init, m, skuRe)) continue;
        std::string sku = m[1].str();

        auto p = priceFrom(card);
        if (name.empty() || sku.empty() || !p) continue;

        std::string skuUpper = sku;
        for (char& c : skuUpper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

        std::map<std::string, slimr::Json> details;
        if (p->exVat) {
            details["Price ex VAT"] = *p->exVat;
        }

        Offer o;
        o.site = "tjomahony";
        o.id = std::move(skuUpper);
        o.url = canonicalUrl(*href, BASE);
        o.name = std::move(name);
        o.price = p->price;
        o.delivery = DELIVERY_UNKNOWN;
        o.details = std::move(details);
        o.checkedAt = std::string(checkedAt);
        offers.push_back(std::move(o));
    }

    std::optional<int> total;
    auto totalEls = doc.querySelectorAll(".toolbar-amount .toolbar-number");
    if (!totalEls.empty()) {
        std::string t = text(totalEls.back().text());
        try {
            int val = std::stoi(t);
            if (val > 0) total = val;
        } catch (...) {}
    }

    return SearchResult{.offers = std::move(offers), .total = total};
}

inline Offer parseTjomahonyProduct(std::string_view html, std::string_view checkedAt) {
    using namespace tjomahony_internal;
    auto doc = Document::parse(html);
    auto blocks = doc.jsonLd();
    auto ldOpt = findLd(blocks, "Product");

    std::string sku;
    if (ldOpt && ldOpt->is_object() && ldOpt->contains("sku")) {
        sku = text((*ldOpt)["sku"]);
    }
    if (sku.empty()) {
        auto form = doc.querySelector("form[data-product-sku]");
        if (form) {
            sku = form->attr("data-product-sku").value_or("");
        }
    }

    auto priceBox = doc.querySelector(".product-info-price");
    auto p = priceBox ? priceFrom(*priceBox) : priceFrom(doc.rootNode());

    if (!ldOpt || sku.empty() || !p) {
        throw std::runtime_error("tjomahony: no product data on the page (layout changed?)");
    }
    const auto& ld = *ldOpt;

    std::map<std::string, std::string> specs;
    auto trs = doc.querySelectorAll("#product-attribute-specs-table tr");
    for (const auto& tr : trs) {
        auto th = tr.querySelector("th");
        auto td = tr.querySelector("td");
        if (th && td) {
            std::string k = text(th->text());
            std::string v = text(td->text());
            if (!k.empty() && !v.empty()) {
                specs[k] = v;
            }
        }
    }

    std::string availability;
    if (ld.is_object() && ld.contains("offers") && ld["offers"].is_object() &&
        ld["offers"].contains("availability") && ld["offers"]["availability"].is_string()) {
        availability = ld["offers"]["availability"].get<std::string>();
    }

    std::string bodyText = doc.body() ? doc.body().text() : "";
    static const std::regex oosRe(R"(out of stock and not available)", std::regex::icase);
    bool outOfStock = std::regex_search(bodyText, oosRe);

    std::optional<bool> inStock;
    if (outOfStock || availability.find("OutOfStock") != std::string::npos) {
        inStock = false;
    } else if (availability.find("InStock") != std::string::npos) {
        inStock = true;
    }

    std::optional<std::string> brand;
    auto bit = specs.find("Brand");
    if (bit != specs.end()) {
        brand = bit->second;
        specs.erase(bit);
    }

    std::string deliveryRule;
    auto rit = specs.find("Delivery Rule");
    if (rit != specs.end()) {
        deliveryRule = rit->second;
        specs.erase(rit);
    }

    std::map<std::string, slimr::Json> details;
    if (p->exVat) {
        details["Price ex VAT"] = *p->exVat;
    }
    for (const auto& [k, v] : specs) {
        details[k] = v;
    }

    if (ld.is_object() && ld.contains("description")) {
        std::string rawDesc = text(ld["description"]);
        auto descDoc = Document::parse("<div>" + rawDesc + "</div>");
        std::string cleanedDesc = descDoc.body() ? text(descDoc.body().text()) : "";
        if (!cleanedDesc.empty()) {
            if (cleanedDesc.size() > 600) cleanedDesc = cleanedDesc.substr(0, 600);
            details["Description"] = cleanedDesc;
        }
    }

    std::optional<std::string> category;
    auto crumbs = findLd(blocks, "BreadcrumbList");
    if (crumbs && crumbs->is_object() && crumbs->contains("itemListElement") && (*crumbs)["itemListElement"].is_array()) {
        auto& arr = (*crumbs)["itemListElement"].get<slimr::Json::array_t>();
        if (arr.size() >= 2) {
            auto& secondLast = arr[arr.size() - 2];
            if (secondLast.is_object() && secondLast.contains("item") && secondLast["item"].is_object() &&
                secondLast["item"].contains("name") && secondLast["item"]["name"].is_string()) {
                category = text(secondLast["item"]["name"].get<std::string>());
            }
        }
    }

    std::string url;
    if (ld.is_object() && ld.contains("url") && ld["url"].is_string()) {
        url = ld["url"].get<std::string>();
    }
    if (url.empty()) {
        auto canon = doc.querySelector("link[rel=\"canonical\"]");
        if (canon) url = canon->attr("href").value_or("");
    }

    std::string name = ld.is_object() && ld.contains("name") && ld["name"].is_string() ? text(ld["name"].get<std::string>()) : "";

    std::optional<std::string> model;
    static const std::regex modelRe(R"(\b([A-Z]{1,5}\d[\w-]*)\b)");
    std::smatch mm;
    if (std::regex_search(name, mm, modelRe)) {
        model = mm[1].str();
    }

    std::string skuUpper = sku;
    for (char& c : skuUpper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    Offer o;
    o.site = "tjomahony";
    o.id = std::move(skuUpper);
    o.url = canonicalUrl(url, BASE);
    o.name = std::move(name);
    o.brand = std::move(brand);
    o.model = std::move(model);
    o.category = std::move(category);
    o.price = p->price;
    o.inStock = inStock;
    o.delivery = deliveryFor(deliveryRule, inStock);
    o.details = std::move(details);
    o.checkedAt = std::string(checkedAt);
    return o;
}

inline std::vector<Pickup> parseTjomahonyStock(std::string_view html, const Near& near) {
    using namespace tjomahony_internal;
    std::string tag = "<script type=\"text/x-magento-init\">";
    size_t at = html.find("\"kickModelSelCollectionBranch\"");
    if (at == std::string_view::npos) {
        throw std::runtime_error("tjomahony: no branch list on the page (layout changed?)");
    }
    size_t start = html.rfind(tag, at);
    if (start == std::string_view::npos) {
        throw std::runtime_error("tjomahony: no branch list on the page (layout changed?)");
    }
    start += tag.size();
    size_t end = html.find("</script>", start);
    if (end == std::string_view::npos) {
        throw std::runtime_error("tjomahony: no branch list on the page (layout changed?)");
    }

    std::string jsonStr(html.substr(start, end - start));
    auto parsed = slimr::parse_json<slimr::Json>(jsonStr);
    if (!parsed || !parsed->is_object()) {
        throw std::runtime_error("tjomahony: failed to parse branch list json");
    }

    auto& root = *parsed;
    if (!root.contains("*") || !root["*"].is_object()) return {};
    auto& star = root["*"];
    if (!star.contains("Magento_Ui/js/core/app") || !star["Magento_Ui/js/core/app"].is_object()) return {};
    auto& app = star["Magento_Ui/js/core/app"];
    if (!app.contains("components") || !app["components"].is_object()) return {};
    auto& comps = app["components"];
    if (!comps.contains("kickModelSelCollectionBranch") || !comps["kickModelSelCollectionBranch"].is_object()) return {};
    auto& branchComp = comps["kickModelSelCollectionBranch"];
    if (!branchComp.contains("config") || !branchComp["config"].is_object()) return {};
    auto& config = branchComp["config"];

    std::optional<std::string> prodIdStr;
    if (config.contains("productId") && config["productId"].is_number()) {
        prodIdStr = std::to_string(static_cast<int64_t>(config["productId"].get<double>()));
    }

    std::vector<Pickup> rows;
    if (config.contains("branchList") && config["branchList"].is_array()) {
        for (const auto& b : config["branchList"].get<slimr::Json::array_t>()) {
            if (!b.is_object()) continue;
            double lat = 0.0, lng = 0.0;
            if (b.contains("lat") && b["lat"].is_string()) {
                try { lat = std::stod(b["lat"].get<std::string>()); } catch (...) {}
            }
            if (b.contains("lng") && b["lng"].is_string()) {
                try { lng = std::stod(b["lng"].get<std::string>()); } catch (...) {}
            }
            double km = std::round(distanceKm(near, lat, lng) * 10.0) / 10.0;
            std::string store = b.contains("name") && b["name"].is_string() ? text(b["name"].get<std::string>()) : "";
            if (store.empty() || !(km <= near.radiusKm)) continue;

            std::optional<int> qty;
            if (b.contains("stock") && b["stock"].is_object()) {
                auto& stockObj = b["stock"];
                if (prodIdStr && stockObj.contains(*prodIdStr) && stockObj[*prodIdStr].is_number()) {
                    qty = static_cast<int>(stockObj[*prodIdStr].get<double>());
                } else if (!stockObj.get<slimr::Json::object_t>().empty()) {
                    auto firstVal = stockObj.get<slimr::Json::object_t>().begin()->second;
                    if (firstVal.is_number()) {
                        qty = static_cast<int>(firstVal.get<double>());
                    }
                }
            }

            std::string status = "unknown";
            if (qty.has_value()) {
                status = (*qty <= 0) ? "out" : (*qty <= 2) ? "low" : "in stock";
            }

            rows.push_back(Pickup{
                .store = std::move(store),
                .km = km,
                .status = std::move(status),
                .qty = qty,
                .readyBy = std::nullopt
            });
        }
    }

    std::sort(rows.begin(), rows.end(), [](const Pickup& a, const Pickup& b) {
        return a.km < b.km;
    });
    return rows;
}

class TjOMahonyAdapter : public SiteAdapter {
public:
    std::string id() const override { return "tjomahony"; }
    std::string matchHint() const override {
        return "TJ O'Mahony matches search words loosely and returns related items too; "
               "check the titles, and try the product type or the model code alone.";
    }

    SearchResult search(SiteContext& ctx, const SearchQuery& query) override {
        std::string enc = slimr::url_encode(query.query);
        for (size_t pos = 0; (pos = enc.find("%20", pos)) != std::string::npos; ) {
            enc.replace(pos, 3, "+");
            pos += 1;
        }
        std::string url = tjomahony_internal::BASE + "catalogsearch/result/?q=" + enc;
        std::string html = ctx.fetchText(url);
        return parseTjomahonySearch(html, "");
    }

    std::vector<Offer> offer(SiteContext& ctx, const OfferRef& ref, const OfferOptions& options) override {
        (void)options;
        std::string url = ref.url.value_or("");
        if (url.empty()) {
            std::string id = ref.id.value_or("");
            if (id.empty()) throw std::runtime_error("tjomahony: give a product id or url");
            auto sr = search(ctx, SearchQuery{.query = id, .filters = {}, .limit = 20});
            std::string idLower = id;
            for (char& c : idLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            for (const auto& o : sr.offers) {
                std::string oIdLower = o.id;
                for (char& c : oIdLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (oIdLower == idLower) {
                    url = o.url;
                    break;
                }
            }
            if (url.empty()) {
                throw std::runtime_error("tjomahony: no product with id " + id + "; search for it first");
            }
        }
        std::string html = ctx.fetchText(canonicalUrl(url, tjomahony_internal::BASE));
        return {parseTjomahonyProduct(html, "")};
    }

    std::vector<Offer> pickup(SiteContext& ctx, const std::vector<Offer>& offers, const Near& near) override {
        std::vector<Offer> out;
        out.reserve(offers.size());
        for (auto offer : offers) {
            std::string html = ctx.fetchText(canonicalUrl(offer.url, tjomahony_internal::BASE));
            offer.pickup = parseTjomahonyStock(html, near);
            out.push_back(std::move(offer));
        }
        return out;
    }
};

} // namespace shop::sites
