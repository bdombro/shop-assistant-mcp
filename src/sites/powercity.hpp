#pragma once

#include "core/adapter.hpp"
#include "core/dom.hpp"
#include "curl.hpp"

#include <algorithm>
#include <cmath>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace shop::sites {

using namespace shop::core;

namespace powercity_internal {

inline const std::string BASE = "https://www.powercity.ie/";
inline const Delivery DELIVERY{
    .status = "available",
    .minDays = 1,
    .maxDays = 2,
    .cost = std::nullopt,
    .source = "site default"
};

inline std::optional<double> price(std::string_view raw) {
    std::string t = text(raw);
    static const std::regex priceRe(R"(€\s*([\d.,]+))");
    std::smatch m;
    if (std::regex_search(t, m, priceRe)) {
        return parsePrice(m[1].str());
    }
    return std::nullopt;
}

inline std::string normalizeKey(std::string_view s) {
    std::string out;
    for (char c : s) {
        if (c >= 'A' && c <= 'Z') out.push_back(static_cast<char>(c + 32));
        else if (c >= 'a' && c <= 'z') out.push_back(c);
    }
    return out;
}

} // namespace powercity_internal

inline SearchResult parsePowercitySearch(std::string_view html, std::string_view checkedAt) {
    using namespace powercity_internal;
    auto doc = Document::parse(html);
    std::vector<Offer> offers;

    auto rows = doc.querySelectorAll("div.group.product");
    for (const auto& row : rows) {
        auto link = row.querySelector("div.productname a[href^=\"/product/\"]");
        if (!link) continue;
        std::string href = link->attr("href").value_or("");
        static const std::regex idRe(R"(/product/([^/?#]+))");
        std::smatch m;
        if (!std::regex_search(href, m, idRe)) continue;
        std::string id = m[1].str();

        auto priceDiv = row.querySelector("div.productprice > div");
        std::optional<double> amount;
        if (priceDiv) {
            amount = price(priceDiv->text());
        }
        if (!amount || *amount <= 0) continue;

        auto lis = row.querySelectorAll("div.productname.desktop-only li");
        std::vector<std::string> lines;
        for (const auto& li : lis) {
            std::string l = text(li.text());
            if (!l.empty()) lines.push_back(std::move(l));
        }

        std::string status;
        std::optional<int> units;
        std::string combined;
        for (size_t i = 0; i < lines.size(); ++i) {
            if (i > 0) combined += " ";
            combined += lines[i];
        }

        static const std::regex statusPrefixRe(R"(^Stock status:\s*)", std::regex::icase);
        static const std::regex qtyRe(R"(Stock\s+(\d+)\s+Available)", std::regex::icase);
        std::smatch qm;
        if (std::regex_search(combined, qm, qtyRe)) {
            units = std::stoi(qm[1].str());
        }

        std::map<std::string, slimr::Json> details;
        static const std::regex kvRe(R"(^([^:]+?)\s*:\s*(.+)$)");
        for (const auto& l : lines) {
            std::smatch sm;
            if (std::regex_search(l, sm, statusPrefixRe)) {
                status = l.substr(sm.length());
            }
            if (std::regex_search(l, sm, kvRe)) {
                std::string k = sm[1].str();
                std::string v = sm[2].str();
                std::string kLower = k;
                for (char& c : kLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (kLower != "stock status") {
                    details[k] = v;
                }
            }
        }
        if (!status.empty()) details["Stock status"] = status;
        if (units) details["Units available"] = int64_t(*units);

        Offer o;
        o.site = "powercity";
        o.id = id;
        o.url = canonicalUrl(BASE + "product/" + id, BASE);
        o.name = text(link->text());
        o.price = Price{.amount = *amount, .currency = "EUR", .was = std::nullopt, .taxIncluded = true};
        if (units) {
            o.inStock = (*units > 0);
        }
        if (units && *units == 0) {
            o.delivery = Delivery{.status = "unavailable", .source = "listing"};
        } else {
            o.delivery = DELIVERY;
        }
        o.details = std::move(details);
        o.checkedAt = std::string(checkedAt);
        offers.push_back(std::move(o));
    }
    return SearchResult{.offers = std::move(offers)};
}

inline std::optional<Offer> parsePowercityProduct(std::string_view html, std::string_view checkedAt) {
    using namespace powercity_internal;
    auto doc = Document::parse(html);

    auto titleEl = doc.querySelector(".producttitle h1");
    std::string name = titleEl ? text(titleEl->text()) : "";

    auto priceEl = doc.querySelector("#pricetag");
    std::optional<double> amount = priceEl ? price(priceEl->text()) : std::nullopt;

    auto allP = doc.querySelectorAll("p");
    auto labelled = [&](std::string_view label) -> std::string {
        for (const auto& p : allP) {
            std::string t = text(p.text());
            if (t.starts_with(label)) {
                std::string res = t.substr(label.size());
                return text(res);
            }
        }
        return "";
    };

    std::string id = labelled("Product Code:");
    if (name.empty() || !amount || id.empty()) {
        return std::nullopt;
    }

    auto homeEl = doc.querySelector(".homedelivery");
    std::string homeText = homeEl ? text(homeEl->text()) : "";
    std::optional<int> units;
    static const std::regex homeRe(R"(Home Delivery\s*:\s*(\d+)\s*Units)", std::regex::icase);
    std::smatch hm;
    if (std::regex_search(homeText, hm, homeRe)) {
        units = std::stoi(hm[1].str());
    }

    std::map<std::string, slimr::Json> details;
    std::string warranty = labelled("Warranty:");
    if (!warranty.empty()) details["Warranty"] = warranty;

    auto features = doc.querySelectorAll("div.product.feature");
    for (const auto& f : features) {
        auto spans = f.querySelectorAll("span");
        if (spans.size() >= 2) {
            std::string k = text(spans[0].text());
            std::string v = text(spans[1].text());
            if (v.starts_with(":")) {
                v = text(v.substr(1));
            }
            if (!k.empty() && !v.empty()) {
                details[k] = v;
            }
        }
    }
    if (units) details["Home delivery units"] = int64_t(*units);

    auto summaryPs = doc.querySelectorAll("#product_information_text p");
    std::string summary;
    static const std::regex ignoreSummary(R"(^Product Infromation)", std::regex::icase);
    for (const auto& p : summaryPs) {
        std::string t = text(p.text());
        if (!t.empty() && !std::regex_search(t, ignoreSummary)) {
            if (!summary.empty()) summary += "; ";
            summary += t;
        }
    }
    if (!summary.empty()) {
        if (summary.size() > 400) summary = summary.substr(0, 400);
        details["Summary"] = summary;
    }

    std::optional<std::string> brand;
    auto bit = details.find("Brand");
    if (bit != details.end() && bit->second.is_string()) {
        brand = bit->second.get<std::string>();
    }

    bool freeDelivery = doc.querySelector(".productfreedel img[alt=\"Free Delivery\"]").has_value();

    Offer o;
    o.site = "powercity";
    o.id = id;
    o.url = canonicalUrl(BASE + "product/" + id, BASE);
    o.name = std::move(name);
    o.brand = std::move(brand);
    o.price = Price{.amount = *amount, .currency = "EUR", .was = std::nullopt, .taxIncluded = true};
    if (units) {
        o.inStock = (*units > 0);
    }
    if (units && *units == 0) {
        o.delivery = Delivery{.status = "unavailable", .source = "product"};
    } else if (freeDelivery) {
        o.delivery = Delivery{
            .status = "available",
            .minDays = 1,
            .maxDays = 2,
            .cost = 0.0,
            .source = "product"
        };
    } else {
        o.delivery = DELIVERY;
    }
    o.details = std::move(details);
    o.checkedAt = std::string(checkedAt);
    return o;
}

inline std::vector<Pickup> parsePowercityStock(std::string_view html, const Near& near) {
    using namespace powercity_internal;
    auto doc = Document::parse(html);

    struct StockRow {
        std::string store;
        int qty = 0;
    };
    std::vector<StockRow> rows;

    auto items = doc.querySelectorAll("div.stockitem");
    for (const auto& el : items) {
        auto storeEl = el.querySelector(".stock.store");
        auto qtyEl = el.querySelector(".stock.qty");
        std::string store = storeEl ? text(storeEl->text()) : "";
        std::string qtyText = qtyEl ? text(qtyEl->text()) : "";
        static const std::regex numRe(R"((\d+))");
        std::smatch m;
        if (!store.empty() && std::regex_search(qtyText, m, numRe)) {
            rows.push_back(StockRow{.store = std::move(store), .qty = std::stoi(m[1].str())});
        }
    }

    std::optional<std::string> readyBy;
    auto h3s = doc.querySelectorAll("h3.clickcollect");
    static const std::regex ccRe(R"(Click & Collect\s*(.+)$)", std::regex::icase);
    for (const auto& h : h3s) {
        std::string t = text(h.text());
        std::smatch m;
        if (std::regex_search(t, m, ccRe)) {
            readyBy = m[1].str();
            break;
        }
    }

    std::vector<Pickup> result;
    for (const auto& s : near.stores) {
        std::string sKey = normalizeKey(s.id);
        const StockRow* matched = nullptr;
        for (const auto& r : rows) {
            std::string rKey = normalizeKey(r.store);
            if (rKey.starts_with(sKey)) {
                matched = &r;
                break;
            }
        }
        double km = std::round(s.km * 10.0) / 10.0;
        if (!matched) {
            result.push_back(Pickup{
                .store = s.name,
                .km = km,
                .status = "unknown",
                .qty = std::nullopt,
                .readyBy = std::nullopt
            });
        } else {
            std::string status = (matched->qty == 0) ? "out" : (matched->qty <= 2) ? "low" : "in stock";
            std::optional<std::string> ready;
            if (matched->qty > 0 && readyBy) {
                ready = readyBy;
            }
            result.push_back(Pickup{
                .store = s.name,
                .km = km,
                .status = std::move(status),
                .qty = matched->qty,
                .readyBy = std::move(ready)
            });
        }
    }
    return result;
}

class PowercityAdapter : public SiteAdapter {
public:
    std::string id() const override { return "powercity"; }
    std::string matchHint() const override {
        return "Keyword search that often jumps to a whole category (\"vacuum\" lists every vacuum). "
               "Use the shop's own words: product type or brand plus model (\"dyson v8\", \"air fryer\").";
    }

    SearchResult search(SiteContext& ctx, const SearchQuery& query) override {
        std::string url = powercity_internal::BASE + "groups/groups/search?search=" + slimr::url_encode(query.query);
        FetchInit init;
        init.document = true;
        std::string html = ctx.fetchText(url, init);
        return parsePowercitySearch(html, "");
    }

    std::vector<Offer> offer(SiteContext& ctx, const OfferRef& ref, const OfferOptions& options) override {
        (void)options;
        std::string id = ref.id.value_or("");
        if (id.empty() && ref.url) {
            static const std::regex prodRe(R"(/product/([^/?#]+))");
            std::smatch m;
            if (std::regex_search(*ref.url, m, prodRe)) {
                id = m[1].str();
            }
        }
        if (id.empty()) {
            throw std::runtime_error("powercity: give an id or a product URL");
        }
        std::string html = ctx.fetchText(powercity_internal::BASE + "product/" + slimr::url_encode(id));
        auto o = parsePowercityProduct(html, "");
        if (!o) {
            throw std::runtime_error("powercity: no product " + id);
        }
        return {*o};
    }

    std::vector<Offer> pickup(SiteContext& ctx, const std::vector<Offer>& offers, const Near& near) override {
        std::vector<Offer> out;
        out.reserve(offers.size());
        for (auto offer : offers) {
            FetchInit init;
            init.headers["x-requested-with"] = "XMLHttpRequest";
            std::string html = ctx.fetchText(powercity_internal::BASE + "product/stock-levels?id=" + slimr::url_encode(offer.id), init);
            offer.pickup = parsePowercityStock(html, near);
            out.push_back(std::move(offer));
        }
        return out;
    }
};

} // namespace shop::sites
