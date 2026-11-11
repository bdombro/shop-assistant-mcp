#pragma once

#include "site_helpers.hpp"

#include <format>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

namespace shop::sites::houseoffraser_ns {

inline const core::Delivery HOUSEOFFRASER_DELIVERY{
    .status = "available",
    .minDays = 3,
    .maxDays = 7,
    .cost = 6.99,
    .source = "site default"
};

inline std::string houseOfFraserProductUrl(std::string_view href, std::string_view id) {
    const std::string BASE = "https://www.houseoffraser.ie/";
    return core::canonicalUrl(href, BASE) + "#colcode=" + std::string(id);
}

inline core::SearchResult parseSearch(std::string_view html, const std::string& checkedAt) {
    auto doc = core::Document::parse(html);
    std::vector<core::Offer> offers;
    const std::string BASE = "https://www.houseoffraser.ie/";

    auto tiles = doc.querySelectorAll("li[li-productid]");
    for (const auto& li : tiles) {
        auto idOpt = li.attr("li-productid");
        auto hrefOpt = li.attr("li-url");
        auto priceOpt = li.attr("li-price");
        if (!idOpt || !hrefOpt || !priceOpt) continue;
        std::string id = *idOpt;
        std::string href = *hrefOpt;
        auto amountOpt = core::parsePrice(*priceOpt);
        if (id.empty() || href.empty() || !amountOpt) continue;
        double amount = *amountOpt;

        auto wasEl = li.querySelector(".RefandPrice");
        std::optional<double> was;
        if (wasEl) {
            was = core::parsePrice(wasEl->text());
        }

        std::map<std::string, slimr::Json> details;
        auto colourOpt = li.attr("li-colour");
        if (colourOpt && !colourOpt->empty()) {
            details["Colour"] = slimr::Json(cleanText(*colourOpt));
        }
        if (li.querySelector(".fromProductPrice")) {
            details["Price"] = slimr::Json("From price: sizes start here");
        }

        std::string category;
        auto subcat = li.attr("li-subcategory");
        if (subcat && !subcat->empty()) {
            category = cleanText(*subcat);
        } else {
            auto cat = li.attr("li-category");
            if (cat && !cat->empty()) category = cleanText(*cat);
        }

        auto nameOpt = li.attr("li-name");
        std::string name = nameOpt ? cleanText(*nameOpt) : "";

        std::optional<std::string> brand;
        auto brandOpt = li.attr("li-brand");
        if (brandOpt && !brandOpt->empty()) {
            std::string b = cleanText(*brandOpt);
            if (!b.empty()) brand = b;
        }

        core::Offer offer;
        offer.site = "houseoffraser";
        offer.id = id;
        offer.url = houseOfFraserProductUrl(href, id);
        offer.name = name;
        if (brand) offer.brand = *brand;
        if (!category.empty()) offer.category = category;
        offer.price = core::Price{
            .amount = amount,
            .currency = "EUR",
            .taxIncluded = true
        };
        if (was && *was > amount) {
            offer.price.was = *was;
        }
        offer.delivery = HOUSEOFFRASER_DELIVERY;
        offer.details = std::move(details);
        offer.checkedAt = checkedAt;

        offers.push_back(std::move(offer));
    }

    std::optional<int> total;
    std::string bodyText = doc.body().text();
    static const std::regex prodCountRe(R"(([\d,]+)\s+products)", std::regex::icase);
    std::smatch m;
    if (std::regex_search(bodyText, m, prodCountRe)) {
        std::string s = m[1].str();
        s.erase(std::remove(s.begin(), s.end(), ','), s.end());
        try {
            total = std::stoi(s);
        } catch (...) {}
    }

    return core::SearchResult{.offers = std::move(offers), .total = total};
}

inline std::vector<core::Offer> parseProduct(std::string_view html, const std::string& checkedAt) {
    auto blocks = core::jsonLdBlocks(html);
    auto product = core::findLd(blocks, "Product");
    if (!product || !product->is_object()) {
        throw std::runtime_error("houseoffraser: no Product JSON-LD on the page (layout changed?)");
    }

    auto doc = core::Document::parse(html);
    auto variantsEl = doc.querySelector("[data-variants]");
    if (!variantsEl) {
        throw std::runtime_error("houseoffraser: no data-variants on the product page (layout changed?)");
    }
    auto rawVariantsOpt = variantsEl->attr("data-variants");
    if (!rawVariantsOpt) {
        throw std::runtime_error("houseoffraser: no data-variants on the product page (layout changed?)");
    }

    auto variantsJson = slimr::parse_json<slimr::Json>(*rawVariantsOpt);
    if (!variantsJson || !variantsJson->is_array()) {
        throw std::runtime_error("houseoffraser: invalid data-variants JSON on the product page");
    }

    const std::string BASE = "https://www.houseoffraser.ie/";
    std::string base;
    if (product->contains("url") && (*product)["url"].is_string()) {
        base = (*product)["url"].get<std::string>();
    } else if (product->contains("@id") && (*product)["@id"].is_string()) {
        base = (*product)["@id"].get<std::string>();
    }
    base = core::canonicalUrl(base, BASE);

    std::string brand;
    if (product->contains("brand")) {
        if ((*product)["brand"].is_string()) {
            brand = cleanText((*product)["brand"].get<std::string>());
        } else if ((*product)["brand"].is_object() && (*product)["brand"].contains("name") && (*product)["brand"]["name"].is_string()) {
            brand = cleanText((*product)["brand"]["name"].get<std::string>());
        }
    }

    std::string fullName = product->contains("name") && (*product)["name"].is_string()
                               ? cleanText((*product)["name"].get<std::string>())
                               : "";
    std::string name = fullName;
    if (!brand.empty() && fullName.starts_with(brand + " ")) {
        name = fullName.substr(brand.length() + 1);
    }

    std::string description = product->contains("description") && (*product)["description"].is_string()
                                  ? cleanText((*product)["description"].get<std::string>())
                                  : "";

    const auto& colours = variantsJson->get<slimr::Json::array_t>();
    std::vector<core::Offer> result;

    for (const auto& c : colours) {
        if (!c.is_object()) continue;
        std::string colVarId = c.contains("ColVarId") && c["ColVarId"].is_string()
                                   ? c["ColVarId"].get<std::string>()
                                   : "";
        std::string colourName = c.contains("ColourName") && c["ColourName"].is_string()
                                     ? cleanText(c["ColourName"].get<std::string>())
                                     : "";

        std::vector<double> sizePrices;
        bool hasInStock = false;
        bool hasSizes = false;
        slimr::Json::object_t sizesMap;

        if (c.contains("SizeVariants") && c["SizeVariants"].is_array()) {
            for (const auto& s : c["SizeVariants"].get<slimr::Json::array_t>()) {
                if (!s.is_object()) continue;
                hasSizes = true;
                std::string sName = s.contains("SizeName") && s["SizeName"].is_string()
                                        ? cleanText(s["SizeName"].get<std::string>())
                                        : "";
                bool inStock = s.contains("InStock") && s["InStock"].is_boolean() ? s["InStock"].as<bool>() : false;
                if (inStock) hasInStock = true;

                std::optional<double> sellPrice;
                if (s.contains("ProdSizePrices") && s["ProdSizePrices"].is_object()) {
                    const auto& psp = s["ProdSizePrices"];
                    if (psp.contains("SellPriceRaw") && psp["SellPriceRaw"].is_number()) {
                        double sp = psp["SellPriceRaw"].as<double>();
                        sellPrice = sp;
                        sizePrices.push_back(sp);
                    }
                }

                std::string sizeDesc;
                if (sellPrice) {
                    sizeDesc = std::format("{} EUR{}", *sellPrice, inStock ? "" : ", out of stock");
                } else {
                    sizeDesc = std::string("? EUR") + (inStock ? "" : ", out of stock");
                }
                if (!sName.empty()) {
                    sizesMap[sName] = slimr::Json(sizeDesc);
                }
            }
        }

        double amount = 0.0;
        if (!sizePrices.empty()) {
            amount = *std::min_element(sizePrices.begin(), sizePrices.end());
        } else if (c.contains("ProdVarPrices") && c["ProdVarPrices"].is_object() &&
                   c["ProdVarPrices"].contains("SellPriceRaw") && c["ProdVarPrices"]["SellPriceRaw"].is_number()) {
            amount = c["ProdVarPrices"]["SellPriceRaw"].as<double>();
        }

        std::optional<double> was;
        // Find cheapest size
        bool foundCheapest = false;
        if (c.contains("SizeVariants") && c["SizeVariants"].is_array()) {
            for (const auto& s : c["SizeVariants"].get<slimr::Json::array_t>()) {
                if (s.is_object() && s.contains("ProdSizePrices") && s["ProdSizePrices"].is_object()) {
                    const auto& psp = s["ProdSizePrices"];
                    if (psp.contains("SellPriceRaw") && psp["SellPriceRaw"].is_number() &&
                        psp["SellPriceRaw"].as<double>() == amount) {
                        bool showRef = psp.contains("ShowRefPrice") && psp["ShowRefPrice"].is_boolean() && psp["ShowRefPrice"].as<bool>();
                        if (showRef && psp.contains("RefPriceRaw") && psp["RefPriceRaw"].is_number()) {
                            was = psp["RefPriceRaw"].as<double>();
                        }
                        foundCheapest = true;
                        break;
                    }
                }
            }
        }
        if (!foundCheapest && c.contains("ProdVarPrices") && c["ProdVarPrices"].is_object()) {
            const auto& pvp = c["ProdVarPrices"];
            bool showRef = pvp.contains("ShowRefPrice") && pvp["ShowRefPrice"].is_boolean() && pvp["ShowRefPrice"].as<bool>();
            if (showRef && pvp.contains("RefPriceRaw") && pvp["RefPriceRaw"].is_number()) {
                was = pvp["RefPriceRaw"].as<double>();
            }
        }

        std::optional<bool> inStock;
        if (hasSizes) inStock = hasInStock;

        std::map<std::string, slimr::Json> details;
        if (!colourName.empty()) details["Colour"] = slimr::Json(colourName);
        if (!sizesMap.empty()) details["Sizes"] = slimr::Json(std::move(sizesMap));
        if (!description.empty()) details["Description"] = slimr::Json(description);

        core::Offer offer;
        offer.site = "houseoffraser";
        offer.id = colVarId;
        offer.url = houseOfFraserProductUrl(base, colVarId);
        offer.name = name;
        if (!brand.empty()) offer.brand = brand;
        offer.price = core::Price{
            .amount = amount,
            .currency = "EUR",
            .taxIncluded = true
        };
        if (was && *was > amount) {
            offer.price.was = *was;
        }
        if (inStock.has_value()) offer.inStock = *inStock;
        if (inStock == false) {
            offer.delivery = core::Delivery{.status = "unavailable", .source = "product"};
        } else {
            offer.delivery = HOUSEOFFRASER_DELIVERY;
        }
        details["Colour"] = slimr::Json(colourName);
        offer.details = std::move(details);

        if (colours.size() > 1) {
            std::string famId = base;
            if (product->contains("inProductGroupWithID") && (*product)["inProductGroupWithID"].is_string()) {
                famId = (*product)["inProductGroupWithID"].get<std::string>();
            }
            offer.family = core::Family{
                .id = famId,
                .size = static_cast<int>(colours.size()),
                .varies = "colour"
            };
        }
        offer.checkedAt = checkedAt;

        result.push_back(std::move(offer));
    }
    return result;
}

class HouseOfFraser : public core::SiteAdapter {
public:
    std::string id() const override { return "houseoffraser"; }
    bool deliveryOnly() const override { return true; }
    std::string matchHint() const override {
        return "House of Fraser matches very loosely and never returns zero rows (nonsense still lists ~100 unrelated items); check names fit the query.";
    }

    core::SearchResult search(core::SiteContext& ctx, const core::SearchQuery& q) override {
        std::string url = "https://www.houseoffraser.ie/searchresults?descriptionfilter=" + urlEncode(q.query);
        std::string html = ctx.fetchText(url);
        return parseSearch(html, "2026-11-10T00:00:00Z");
    }

    std::vector<core::Offer> offer(core::SiteContext& ctx, const core::OfferRef& ref, const core::OfferOptions& options) override {
        const std::string BASE = "https://www.houseoffraser.ie/";
        std::string url = ref.url.value_or("");
        std::string id = ref.id.value_or("");
        if (id.empty() && !url.empty()) {
            static const std::regex colRe(R"(colcode=(\d+))");
            std::smatch m;
            if (std::regex_search(url, m, colRe)) {
                id = m[1].str();
            }
        }
        if (url.empty()) {
            std::string searchUrl = BASE + "searchresults?descriptionfilter=" + urlEncode(id);
            auto rows = parseSearch(ctx.fetchText(searchUrl), "").offers;
            for (const auto& o : rows) {
                if (o.id == id) {
                    url = o.url;
                    break;
                }
            }
            if (url.empty()) {
                throw std::runtime_error("houseoffraser: no product with colour id " + id);
            }
        }

        std::string html = ctx.fetchText(core::canonicalUrl(url, BASE));
        auto offers = parseProduct(html, "2026-11-10T00:00:00Z");
        if (id.empty() && !offers.empty()) {
            id = offers[0].id;
        }

        const core::Offer* asked = nullptr;
        for (const auto& o : offers) {
            if (o.id == id) {
                asked = &o;
                break;
            }
        }
        if (!asked && !offers.empty()) {
            asked = &offers[0];
        }
        if (!asked) {
            throw std::runtime_error("houseoffraser: no variants on the product page");
        }

        if (options.family) {
            std::vector<core::Offer> res;
            res.push_back(*asked);
            for (const auto& o : offers) {
                if (o.id != asked->id) {
                    res.push_back(o);
                }
            }
            return res;
        }
        return {*asked};
    }
};

} // namespace shop::sites::houseoffraser_ns
namespace shop::sites {
using HouseOfFraser = houseoffraser_ns::HouseOfFraser;
using HouseOfFraserAdapter = houseoffraser_ns::HouseOfFraser;
} // namespace shop::sites
