#pragma once

#include "site_utils.hpp"

namespace shop::sites {

inline const Delivery BROOKS_DELIVERY{ "available", 2, 4, std::nullopt, "site default" };

inline std::vector<Pickup> parseBrooksBranches(std::string_view html, const Near& near) {
    static const std::regex markerRe(R"("branchList":\s*\[)");
    std::string s(html);
    std::sregex_iterator it(s.begin(), s.end(), markerRe);
    std::sregex_iterator end;

    size_t matchPos = std::string::npos;
    size_t markerLen = 0;
    for (; it != end; ++it) {
        size_t idx = it->position();
        std::string sample = s.substr(idx, std::min<size_t>(s.size() - idx, 800));
        if (sample.find("\"stock\"") != std::string::npos) {
            matchPos = idx;
            markerLen = it->length();
            break;
        }
    }

    if (matchPos == std::string::npos) {
        throw std::runtime_error("brooks: no branch stock on the product page (layout changed?)");
    }

    size_t start = matchPos + markerLen - 1; // pointing at '['
    int depth = 0;
    bool inString = false;
    size_t endPos = std::string::npos;
    for (size_t i = start; i < s.size(); ++i) {
        char c = s[i];
        if (inString) {
            if (c == '\\') {
                i++;
            } else if (c == '"') {
                inString = false;
            }
        } else if (c == '"') {
            inString = true;
        } else if (c == '[') {
            depth++;
        } else if (c == ']') {
            if (--depth == 0) {
                endPos = i;
                break;
            }
        }
    }

    if (endPos == std::string::npos) {
        throw std::runtime_error("brooks: unterminated branch list on the product page");
    }

    std::string jsonStr = s.substr(start, endPos - start + 1);
    auto parsed = slimr::parse_json<slimr::Json>(jsonStr);
    if (!parsed || !parsed->is_array()) {
        throw std::runtime_error("brooks: unterminated branch list on the product page");
    }

    std::vector<Pickup> rows;
    for (const auto& b : parsed->get<slimr::Json::array_t>()) {
        if (!b.is_object()) continue;
        std::string name = b.contains("name") ? jsonString(b["name"]) : "";
        std::string latStr = b.contains("lat") ? jsonString(b["lat"]) : "";
        std::string lngStr = b.contains("lng") ? jsonString(b["lng"]) : "";
        if (latStr.empty() || lngStr.empty()) continue;

        double bLat = 0.0, bLon = 0.0;
        try {
            bLat = std::stod(latStr);
            bLon = std::stod(lngStr);
        } catch (...) {
            continue;
        }

        double km = distanceKm(near.lat, near.lon, bLat, bLon);
        if (km > near.radiusKm) continue;

        std::optional<int> qty;
        if (b.contains("stock") && b["stock"].is_object()) {
            for (const auto& [_, qVal] : b["stock"].get<slimr::Json::object_t>()) {
                if (qVal.is_number()) {
                    qty = static_cast<int>(qVal.get_number());
                    break;
                }
            }
        }

        Pickup p;
        p.store = name;
        p.km = km;
        if (!qty.has_value()) {
            p.status = "unknown";
        } else if (*qty > 0) {
            p.status = "in stock";
            p.qty = *qty;
            p.readyBy = "today (about 2 hours, in opening hours)";
        } else {
            p.status = "out";
            p.qty = 0;
        }
        rows.push_back(std::move(p));
    }

    std::sort(rows.begin(), rows.end(), [](const Pickup& a, const Pickup& b) {
        return a.km < b.km;
    });
    return rows;
}

inline SearchResult parseBrooksSearch(std::string_view html, std::string_view checkedAt) {
    auto document = Document::parse(html);
    const std::string BASE = "https://brooksonline.ie/";

    auto items = document.querySelectorAll("li.product-item");
    if (items.empty()) {
        auto body = document.body();
        std::string bText = cleanText(body);
        static const std::regex noResRe(R"(returned no results)", std::regex::icase);
        if (std::regex_search(bText, noResRe)) {
            return SearchResult{ {}, std::nullopt, {} };
        }
        throw std::runtime_error("brooks: no product grid on the search page (layout changed?)");
    }

    std::vector<Offer> offers;
    for (const auto& li : items) {
        auto link = li.querySelector("a.product-name");
        std::string href = link ? link->attr("href").value_or("") : "";

        std::string sku;
        auto mageInit = li.querySelector("[data-mage-init*=\"productSku\"]");
        if (mageInit) {
            std::string attrVal = mageInit->attr("data-mage-init").value_or("");
            static const std::regex skuRe(R"re("productSku":\s*"([^"]+)")re");
            std::smatch sm;
            if (std::regex_search(attrVal, sm, skuRe)) {
                sku = sm[1].str();
            }
        }

        auto priceBox = li.querySelector(".price-box");
        std::optional<double> amount;
        if (priceBox) {
            amount = parsePrice(cleanText(priceBox->querySelector(".price-including-tax .price")));
        }
        if (href.empty() || sku.empty() || !amount.has_value()) continue;

        std::optional<double> exVat;
        std::optional<double> was;
        if (priceBox) {
            exVat = parsePrice(cleanText(priceBox->querySelector(".price-excluding-tax .price")));
            was = parsePrice(cleanText(priceBox->querySelector(".old-price .price")));
        }

        std::string name = cleanText(link);
        auto quantity = parseQuantity(name);
        std::string deliveryStr = cleanText(li.querySelector(".delivery-status"));

        Delivery del = BROOKS_DELIVERY;
        static const std::regex unavailRe(R"(unavailable|not available)", std::regex::icase);
        if (std::regex_search(deliveryStr, unavailRe)) {
            del = Delivery{ "unavailable", std::nullopt, std::nullopt, std::nullopt, "listing" };
        }

        std::map<std::string, slimr::Json> details;
        if (exVat.has_value()) {
            details["Price ex VAT"] = slimr::Json(*exVat);
        }

        Offer o;
        o.site = "brooks";
        o.id = sku;
        o.url = canonicalUrl(href, BASE);
        o.name = name;
        o.price = Price{ *amount, "EUR", (was.has_value() && *was > *amount) ? was : std::nullopt, true };
        if (quantity.has_value()) o.quantity = quantity;
        o.delivery = del;
        o.details = std::move(details);
        o.checkedAt = std::string(checkedAt);
        offers.push_back(std::move(o));
    }

    std::optional<int> total;
    auto toolbarNum = document.querySelector("#toolbar-amount .toolbar-number:last-child");
    if (toolbarNum) {
        auto val = parsePrice(cleanText(toolbarNum));
        if (val.has_value() && *val > 0) total = static_cast<int>(*val);
    }

    return SearchResult{ std::move(offers), total, {} };
}

inline Offer parseBrooksProduct(std::string_view html, std::string_view checkedAt) {
    auto document = Document::parse(html);
    const std::string BASE = "https://brooksonline.ie/";

    auto product = findLd(document.jsonLd(), "Product");
    if (!product || !product->is_object()) {
        throw std::runtime_error("brooks: no Product JSON-LD on the page (layout changed?)");
    }

    slimr::Json ldOffer;
    if (product->contains("offers") && (*product)["offers"].is_object()) {
        ldOffer = (*product)["offers"];
    }

    std::string sku = cleanText(document.querySelector("[itemprop=\"sku\"]"));
    auto amount = parsePrice(cleanText(document.querySelector(".price-including-tax .price")));
    if (!amount.has_value() && ldOffer.is_object() && ldOffer.contains("price")) {
        amount = parsePrice(ldOffer["price"]);
    }
    if (sku.empty() || !amount.has_value()) {
        throw std::runtime_error("brooks: no product code or price on the page (layout changed?)");
    }

    auto exVat = parsePrice(cleanText(document.querySelector(".price-excluding-tax .price")));
    auto was = parsePrice(cleanText(document.querySelector(".old-price .price")));

    std::map<std::string, slimr::Json> details;
    for (const auto& tr : document.querySelectorAll("#product-attribute-specs-table tr")) {
        std::string label = cleanText(tr.querySelector("th"));
        std::string val = cleanText(tr.querySelector("td"));
        if (!label.empty() && !val.empty()) {
            details[label] = slimr::Json(val);
        }
    }
    if (exVat.has_value()) {
        details["Price ex VAT"] = slimr::Json(*exVat);
    }

    std::string lead = cleanText(document.querySelector("#delivery_collection_options .delivery .stock-amount"));
    if (!lead.empty()) {
        details["Delivery time"] = slimr::Json(lead);
    }

    static const std::regex daysRe(R"((\d+)\s*working days)", std::regex::icase);
    std::smatch dm;
    std::optional<int> days;
    if (std::regex_search(lead, dm, daysRe)) {
        try { days = std::stoi(dm[1].str()); } catch (...) {}
    }

    std::optional<bool> inStock;
    if (ldOffer.is_object() && ldOffer.contains("availability")) {
        std::string avail = jsonString(ldOffer["availability"]);
        static const std::regex schemaRe(R"(^https?://schema\.org/)");
        avail = std::regex_replace(avail, schemaRe, "");
        inStock = parseStock(avail);
    }

    std::string name = cleanText(jsonString((*product)["name"]));
    auto quantity = parseQuantity(name);
    std::optional<std::string> brand;
    if (details.contains("Brand") && details["Brand"].is_string()) {
        brand = details["Brand"].get<std::string>();
    }

    std::string productUrl;
    if (ldOffer.is_object() && ldOffer.contains("url")) {
        productUrl = jsonString(ldOffer["url"]);
    }
    if (productUrl.empty() && product->contains("url")) {
        productUrl = jsonString((*product)["url"]);
    }

    Delivery del = BROOKS_DELIVERY;
    if (inStock.has_value() && !*inStock) {
        del = Delivery{ "unavailable", std::nullopt, std::nullopt, std::nullopt, "product" };
    } else if (days.has_value()) {
        del = Delivery{ "available", *days, *days, std::nullopt, "product" };
    }

    Offer o;
    o.site = "brooks";
    o.id = sku;
    o.url = canonicalUrl(productUrl, BASE);
    o.name = name;
    if (brand.has_value()) o.brand = *brand;
    o.price = Price{ *amount, "EUR", (was.has_value() && *was > *amount) ? was : std::nullopt, true };
    if (quantity.has_value()) o.quantity = quantity;
    if (inStock.has_value()) o.inStock = *inStock;
    o.delivery = del;
    o.details = std::move(details);
    o.checkedAt = std::string(checkedAt);
    return o;
}

class BrooksAdapter : public SiteAdapter {
public:
    std::string id() const override { return "brooks"; }

    std::string matchHint() const override {
        return "Brooks search is loose and returns related items; use the product type and size in the shop's wording, or a product code.";
    }

    SearchResult search(SiteContext& ctx, const SearchQuery& q) override {
        const std::string BASE = "https://brooksonline.ie/";
        std::string enc = urlEncode(q.query);
        // replace %20 with +
        for (size_t i = 0; i + 2 < enc.size(); ++i) {
            if (enc[i] == '%' && enc[i+1] == '2' && enc[i+2] == '0') {
                enc.replace(i, 3, "+");
            }
        }
        std::string url = BASE + "catalogsearch/result/?q=" + enc;
        std::string html = ctx.fetchText(url);
        return parseBrooksSearch(html, "2026-10-16T00:00:00Z");
    }

    std::vector<Offer> offer(SiteContext& ctx, const OfferRef& ref, const OfferOptions& options) override {
        (void)options;
        const std::string BASE = "https://brooksonline.ie/";
        std::string url = ref.url.value_or("");
        if (url.empty() && ref.id.has_value()) {
            auto searchRes = search(ctx, SearchQuery{ *ref.id, {}, 5 });
            for (const auto& o : searchRes.offers) {
                if (o.id == *ref.id) {
                    url = o.url;
                    break;
                }
            }
            if (url.empty()) {
                throw std::runtime_error("brooks: no product with code " + *ref.id);
            }
        }
        if (url.empty()) {
            throw std::runtime_error("brooks: offer needs an id or url");
        }
        std::string html = ctx.fetchText(canonicalUrl(url, BASE));
        return { parseBrooksProduct(html, "2026-10-16T00:00:00Z") };
    }

    std::vector<Offer> pickup(SiteContext& ctx, const std::vector<Offer>& offers, const Near& near) override {
        std::vector<Offer> out;
        for (const auto& offer : offers) {
            std::string html = ctx.fetchText(offer.url);
            Offer copy = offer;
            copy.pickup = parseBrooksBranches(html, near);
            out.push_back(std::move(copy));
        }
        return out;
    }
};

} // namespace shop::sites
