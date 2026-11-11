#pragma once

#include "test.hpp"
#include "sites/chadwicks.hpp"
#include "sites/currys.hpp"
#include "sites/diy.hpp"
#include "sites/donedeal.hpp"
#include "sites/mcmahons.hpp"
#include "sites/mcquillantools.hpp"
#include "sites/mydiy.hpp"
#include "sites/woodies.hpp"
#include "sites/all_sites.hpp"

TEST_CASE("all newly ported site adapters parse search and product HTML") {
    std::string checkedAt = "2026-11-10T00:00:00Z";

    // Chadwicks search json
    std::string chadwicksJson = R"({"result":[{"name":"Chadwicks Timber 2x4","url":"/timber-2x4","price":15.99,"sku":"CH-123"}]})";
    auto cRes = shop::sites::parseChadwicksSearch(chadwicksJson, checkedAt);
    CHECK(cRes.offers.size() == 1);
    CHECK(cRes.offers[0].name == "Chadwicks Timber 2x4");
    CHECK(cRes.offers[0].price.amount == 15.99);

    // Currys search html
    std::string currysHtml = R"(<html><body><div class='product-tile'><a class='product-title' href='/laptop'>Gaming Laptop</a><span class='price'>€999.99</span></div></body></html>)";
    auto curRes = shop::sites::parseCurrysSearch(currysHtml, checkedAt);
    CHECK(curRes.offers.size() == 1);
    CHECK(curRes.offers[0].name == "Gaming Laptop");
    CHECK(curRes.offers[0].price.amount == 999.99);

    // DIY search html
    std::string diyHtml = R"(<html><body><div class='product-card'><a href='/drill'>Cordless Drill</a><span class='price'>€89.00</span></div></body></html>)";
    auto diyRes = shop::sites::parseDiySearch(diyHtml, checkedAt);
    CHECK(diyRes.offers.size() == 1);
    CHECK(diyRes.offers[0].name == "Cordless Drill");
    CHECK(diyRes.offers[0].price.amount == 89.00);

    // DoneDeal search html
    std::string ddHtml = R"(<html><body><article><a href='/ads/123'>Used Van</a><span class='price'>€5,000</span></article></body></html>)";
    auto ddRes = shop::sites::parseDonedealSearch(ddHtml, checkedAt);
    CHECK(ddRes.offers.size() == 1);
    CHECK(ddRes.offers[0].name == "Used Van");
    CHECK(ddRes.offers[0].price.amount == 5000.0);

    // McQuillan Tools search html
    std::string mcqHtml = R"(<html><body><div class='product-item'><a class='product-item-link' href='/hammer'>Claw Hammer</a><span class='price'>€24.50</span></div></body></html>)";
    auto mcqRes = shop::sites::parseMcquillanSearch(mcqHtml, checkedAt);
    CHECK(mcqRes.offers.size() == 1);
    CHECK(mcqRes.offers[0].name == "Claw Hammer");
    CHECK(mcqRes.offers[0].price.amount == 24.50);

    // MyDIY search html
    std::string mydHtml = R"(<html><body><div class='product-item'><a class='woocommerce-LoopProduct-link' href='/paint'>White Paint</a><span class='price'>€35.00</span></div></body></html>)";
    auto mydRes = shop::sites::parseMydiySearch(mydHtml, checkedAt);
    CHECK(mydRes.offers.size() == 1);
    CHECK(mydRes.offers[0].name == "White Paint");
    CHECK(mydRes.offers[0].price.amount == 35.00);

    // Woodies search html
    std::string woodHtml = R"(<html><body><div class='product-tile'><a class='product-link' href='/lawnmower'>Lawnmower</a><span class='price'>€299.00</span></div></body></html>)";
    auto woodRes = shop::sites::parseWoodiesSearch(woodHtml, checkedAt);
    CHECK(woodRes.offers.size() == 1);
    CHECK(woodRes.offers[0].name == "Lawnmower");
    CHECK(woodRes.offers[0].price.amount == 299.00);

    auto& reg = shop::core::AdapterRegistry::instance();
    shop::sites::register_all_sites(reg);
    CHECK(reg.allIds().size() == 22);
}
