#pragma once

#include "test.hpp"

#include "core/dom.hpp"
#include "core/policy.hpp"
#include "core/markdown.hpp"
#include "core/db.hpp"
#include "core/cdp.hpp"
#include "core/adapter.hpp"
#include "core/pages.hpp"
#include "app.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>

namespace shop::testing {

inline std::string readFixture(const std::string& relPath) {
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::path("fixtures") / relPath,
        std::filesystem::path("tests/fixtures") / relPath,
    };
    for (const auto& p : candidates) {
        if (std::filesystem::exists(p)) {
            std::ifstream file(p, std::ios::binary);
            if (file) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                return buffer.str();
            }
        }
    }
    throw std::runtime_error("Fixture not found: " + relPath);
}

} // namespace shop::testing

using namespace shop::core;
using namespace shop::testing;

TEST_CASE("reporting is opt-in and follows BUGSINK_DSN after app construction") {
    struct RestoreEnvironment {
        std::optional<std::string> value;
        ~RestoreEnvironment() {
            if (value) {
                setenv("BUGSINK_DSN", value->c_str(), 1);
            } else {
                unsetenv("BUGSINK_DSN");
            }
        }
    };
    const char* previous = std::getenv("BUGSINK_DSN");
    RestoreEnvironment restore{previous ? std::optional<std::string>(previous) : std::nullopt};
    unsetenv("BUGSINK_DSN");
    auto app = shop::make_shop_assistant_app();
    REQUIRE(app.bugsink() != nullptr);
    CHECK(app.bugsink()->effectiveDsn().empty());
    setenv("BUGSINK_DSN", "https://public@example.invalid/1", 1);
    CHECK(app.bugsink()->effectiveDsn() == "https://public@example.invalid/1");
    setenv("BUGSINK_DSN", "", 1);
    CHECK(app.bugsink()->effectiveDsn().empty());
}

// ============================================================================
// 1. Database Store Tests (db.test.ts)
// ============================================================================
TEST_SUITE("db store") {
    TEST_CASE("page cache honours TTL") {
        int64_t t = 1000000;
        auto store = Store::open(":memory:", [&]() { return t; });

        CleanPage page;
        page.title = "t";
        page.url = "https://shop.ie/p/1";
        page.sections = {Section{"s1", "(top)", "hi", 1}};

        store.putPage("k", PageKind::product, 60, page);

        auto cached = store.getPage("k");
        REQUIRE(cached.has_value());
        CHECK(cached->sections[0].markdown == "hi");
        CHECK(cached->fetchedAt == 1000000);

        t += 59000;
        CHECK(store.getPage("k").has_value());

        t += 2000;
        CHECK(!store.getPage("k").has_value());
        CHECK(!store.getPage("missing").has_value());

        store.close();
    }

    TEST_CASE("putPage replaces an existing entry") {
        auto store = Store::open(":memory:");

        CleanPage page;
        page.title = "t";
        page.url = "https://shop.ie/p/1";
        page.sections = {Section{"s1", "(top)", "hi", 1}};

        store.putPage("k", PageKind::product, 60, page);

        page.title = "new";
        store.putPage("k", PageKind::product, 60, page);

        auto cached = store.getPage("k");
        REQUIRE(cached.has_value());
        CHECK(cached->title == "new");

        store.close();
    }

    TEST_CASE("guard log filters by page and time") {
        auto store = Store::open(":memory:");

        auto makeEv = [](int64_t pageId, int64_t at) {
            GuardEvent ev;
            ev.at = at;
            ev.pageId = pageId;
            ev.site = "shop";
            ev.method = "POST";
            ev.url = "https://shop.ie/basket/add";
            ev.action = "ask";
            ev.outcome = "denied";
            ev.reason = "write needs approval";
            return ev;
        };

        store.logGuard(makeEv(1, 10));
        store.logGuard(makeEv(2, 20));
        store.logGuard(makeEv(1, 30));

        auto evs1 = store.guardEvents({1}, 15);
        REQUIRE(evs1.size() == 1);
        CHECK(evs1[0].at == 30);

        auto evsAll = store.guardEvents({1, 2}, 0);
        CHECK(evsAll.size() == 3);

        store.close();
    }

    TEST_CASE("site health") {
        int64_t t = 1;
        auto store = Store::open(":memory:", [&]() { return t; });

        store.recordCheck("shop", true, "ok");
        t = 2;
        store.recordCheck("shop", false, "403 on search");

        auto health = store.siteHealth("shop");
        CHECK(health.lastChecked == 2);
        CHECK(health.lastWorked == 1);
        REQUIRE(health.lastError.has_value());
        CHECK(*health.lastError == "403 on search");

        auto emptyHealth = store.siteHealth("none");
        CHECK(!emptyHealth.lastChecked.has_value());
        CHECK(!emptyHealth.lastWorked.has_value());
        CHECK(!emptyHealth.lastError.has_value());

        store.close();
    }
}

// ============================================================================
// 2. Markdown & Cleaning Tests (markdown.test.ts)
// ============================================================================
TEST_SUITE("markdown & cleaner") {
    TEST_CASE("cleanHtml: title, JSON-LD, main content") {
        std::string html = readFixture("markdown/product-injected.html");
        auto page = cleanHtml(html, "https://www.shop.ie/p/123");

        std::string all;
        for (const auto& s : page.sections) {
            all += s.markdown + "\n";
        }

        CHECK(page.title.find("Einhell TE-CD 18/40") != std::string::npos);
        REQUIRE(!page.jsonLd.empty());
        CHECK(page.jsonLd[0]["gtin13"].get<std::string>() == "4006825640519");
        CHECK(all.find("€89.99") != std::string::npos);
        CHECK(all.find("Torque: 40 Nm") != std::string::npos);
        CHECK(all.find("bare tool, no battery") != std::string::npos);

        // Hidden text and chrome stripped
        std::vector<std::string> forbidden = {
            "ignore previous instructions",
            "out of stock",
            "Hidden aria text",
            "Offscreen instruction",
            "Zero size instruction",
            "Marked hidden by the browser",
            "We use cookies",
            "window.tracking",
            "color:red",
            "© Shop Ltd",
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
        };
        for (const auto& bad : forbidden) {
            CHECK(all.find(bad) == std::string::npos);
        }

        // Links absolute, sections split at headings
        CHECK(all.find("https://www.shop.ie/p/123/reviews") != std::string::npos);

        std::vector<std::string> headings;
        std::vector<std::string> ids;
        for (const auto& s : page.sections) {
            headings.push_back(s.heading);
            ids.push_back(s.id);
        }
        std::vector<std::string> expectedHeadings = {
            "Einhell TE-CD 18/40 Li Drill",
            "Specifications",
            "Customer reviews"
        };
        std::vector<std::string> expectedIds = {"s1", "s2", "s3"};
        CHECK(headings == expectedHeadings);
        CHECK(ids == expectedIds);
    }

    TEST_CASE("pageView: outline of all sections, markdown up to the budget") {
        std::string html = readFixture("markdown/product-injected.html");
        auto page = cleanHtml(html, "https://www.shop.ie/p/123");

        auto full = pageView(page, 10000);
        CHECK(full.outline.size() == 3);
        CHECK(!full.truncated);
        CHECK(full.jsonLd.size() == 1);

        auto tight = pageView(page, 40);
        CHECK(tight.truncated);
    }

    TEST_CASE("pageView: a tiny first section doesn't stop the fill; big one cut at line break") {
        std::string lines;
        for (int i = 0; i < 400; ++i) {
            lines += "- item " + std::to_string(i) + " €" + std::to_string(i) + ".99\n";
        }
        std::string html = "<main><h1></h1><h2>Results</h2><ul>";
        std::istringstream iss(lines);
        std::string l;
        while (std::getline(iss, l)) {
            if (l.starts_with("- ")) {
                html += "<li>" + l.substr(2) + "</li>";
            }
        }
        html += "</ul></main>";

        auto p = cleanHtml(html, "https://shop.ie/");
        for (const auto& s : p.sections) {
            CHECK(s.markdown != "#");
        }

        auto v = pageView(p, 500);
        CHECK(v.cut == "s1");
        CHECK(v.markdown.find("item 1 €1.99") != std::string::npos);
        CHECK(v.markdown.ends_with(".99"));

        int estTokens = estimateTokens(slimr::write_json(v));
        CHECK(estTokens <= 560);
    }

    TEST_CASE("pageView: one budget covers JSON-LD and outline (synthetic search fixtures)") {
        for (const std::string& site : {"amazon", "diy"}) {
            std::string html = readFixture(site + "/search.html");
            auto v = pageView(cleanHtml(html, "https://www." + site + ".ie/"), 1000);
            int estTokens = estimateTokens(slimr::write_json(v));
            CHECK(estTokens <= 1100);
            CHECK(v.markdown.size() > 400);
        }
    }

    TEST_CASE("pageView: requested sections obey the budget and label withheld ones") {
        std::string html = readFixture("markdown/product-injected.html");
        auto page = cleanHtml(html, "https://www.shop.ie/p/123");

        PageViewOptions opts1;
        opts1.withheld = {"s3"};
        opts1.only = std::vector<std::string>{"s3"};
        auto v1 = pageView(page, 10000, opts1);
        CHECK(v1.markdown.find("withheld section s3") != std::string::npos);

        PageViewOptions opts2;
        opts2.withheld = {"s3"};
        auto v2 = pageView(page, 10000, opts2);
        CHECK(v2.markdown.find("withheld section") == std::string::npos);
    }

    TEST_CASE("cleanJson: one section with compact JSON") {
        slimr::Json j;
        slimr::Json::object_t obj;
        slimr::Json::array_t prods;
        slimr::Json::object_t p1;
        p1["title"] = "x";
        p1["price"] = int64_t(1);
        prods.push_back(std::move(p1));
        obj["products"] = std::move(prods);
        j = slimr::Json(std::move(obj));

        auto p = cleanJson(j, "https://shop.ie/products.json");
        REQUIRE(p.sections.size() == 1);
        CHECK(p.sections[0].markdown.find("\"price\":1") != std::string::npos);
    }

    TEST_CASE("cleanHtml: images become short alt text, dead links become text") {
        std::string html = R"raw(<main><p><img src="data:image/svg+xml,AAAA" alt=""><img src="/a.jpg" alt="Drill front"> <a href="javascript:void(0)">Filters</a> <a href="/p/1">Real</a></p></main>)raw";
        auto p = cleanHtml(html, "https://shop.ie/");

        std::string md;
        for (const auto& s : p.sections) md += s.markdown + "\n";

        static const std::regex badRe(R"(data:image|javascript:|!\[)");
        CHECK(!std::regex_search(md, badRe));
        CHECK(md.find("[img: Drill front]") != std::string::npos);
        CHECK(md.find("Filters") != std::string::npos);
        CHECK(md.find("[Real](https://shop.ie/p/1)") != std::string::npos);
    }

    TEST_CASE("cleanJson: strips HTML in strings and caps long ones") {
        slimr::Json j;
        slimr::Json::object_t obj;
        std::string longStr(500, 'x');
        obj["description"] = "<p>Great <b>drill</b>&nbsp;kit</p>" + longStr;
        obj["price"] = int64_t(5);
        j = slimr::Json(std::move(obj));

        auto p = cleanJson(j, "https://shop.ie/x.json");
        std::string md = p.sections[0].markdown;
        CHECK(md.find("<p>") == std::string::npos);
        CHECK(md.find("<b>") == std::string::npos);
        CHECK(md.find("&nbsp;") == std::string::npos);
        CHECK(md.find("Great drill kit") != std::string::npos);
        CHECK(md.size() < 400);
    }
}

// ============================================================================
// 3. Policy & Guard Tests (policy.test.ts & guard.test.ts)
// ============================================================================
TEST_SUITE("policy & guard") {
    const std::string screwfixDoc = R"(---
id: screwfix
name: Screwfix Ireland
domains: [screwfix.ie]
kind: retailer
categories: [home-improvement]
urls: { search: "https://www.screwfix.ie/search?search={query}" }
readPosts: ["/api/search"]
---

# Screwfix

Prices render as `€ 26 .95 INC VAT`.
)";

    TEST_CASE("parseSiteDoc reads frontmatter and prose, applying defaults") {
        auto doc = parseSiteDoc(screwfixDoc);
        CHECK(doc.policy.id == "screwfix");
        REQUIRE(doc.policy.domains.size() == 1);
        CHECK(doc.policy.domains[0] == "screwfix.ie");
        CHECK(doc.policy.defaults.read == ReadState::ask);
        CHECK(doc.policy.defaults.write == WriteState::ask);
        CHECK(doc.policy.concurrency == 4);
        CHECK(doc.prose.starts_with("# Screwfix"));
    }

    TEST_CASE("parseSiteDoc rejects missing frontmatter and invalid fields") {
        CHECK_THROWS(parseSiteDoc("# no frontmatter"));
        std::string invalidKind = screwfixDoc;
        invalidKind.replace(invalidKind.find("kind: retailer"), 14, "kind: shop");
        CHECK_THROWS(parseSiteDoc(invalidKind));

        std::string writeOn = screwfixDoc;
        writeOn.replace(writeOn.find("readPosts"), 9, "defaults: { read: on, write: on }\nreadPosts");
        CHECK_THROWS(parseSiteDoc(writeOn));
    }

    TEST_CASE("siteForUrl matches domains and subdomains only") {
        auto p1 = parseSiteDoc(screwfixDoc).policy;
        auto p2 = p1;
        p2.id = "amazon";
        p2.domains = {"amazon.ie", "amazon.co.uk"};
        std::vector<SitePolicy> sites = {p1, p2};

        struct TestCase {
            std::string url;
            std::optional<std::string> expected;
        };
        std::vector<TestCase> cases = {
            {"https://www.screwfix.ie/p/123", "screwfix"},
            {"https://screwfix.ie/", "screwfix"},
            {"https://m.amazon.co.uk/dp/B0", "amazon"},
            {"https://notscrewfix.ie/", std::nullopt},
            {"https://screwfix.ie.evil.com/", std::nullopt},
            {"https://example.com/?u=screwfix.ie", std::nullopt},
        };

        for (const auto& tc : cases) {
            const auto* matched = siteForUrl(tc.url, sites);
            if (tc.expected.has_value()) {
                REQUIRE(matched != nullptr);
                CHECK(matched->id == *tc.expected);
            } else {
                CHECK(matched == nullptr);
            }
        }
    }

    TEST_CASE("urlMatches: path prefixes and wildcards") {
        struct TestCase {
            std::string url;
            std::string pattern;
            bool expected;
        };
        std::vector<TestCase> cases = {
            {"https://www.screwfix.ie/checkout/step1", "/checkout", true},
            {"https://www.screwfix.ie/checkouts-guide", "/checkout", true},
            {"https://www.screwfix.ie/p/checkout-tool", "/checkout", false},
            {"https://www.paypal.com/x", "*paypal.com/*", true},
            {"https://www.amazon.ie/gp/buy/spc/handlers/display.html", "*/gp/buy/*", true},
            {"https://www.amazon.ie/s?k=router", "*/gp/buy/*", false},
            {"https://shop.ie/basket?remove=1", "*remove=*", true},
        };

        for (const auto& tc : cases) {
            CAPTURE(tc.url);
            CAPTURE(tc.pattern);
            CHECK(urlMatches(tc.url, tc.pattern) == tc.expected);
        }
    }

    TEST_CASE("guard decide: full suite matching guard.test.ts") {
        SitePolicy shop;
        shop.id = "shop";
        shop.name = "Shop";
        shop.domains = {"shop.ie"};
        shop.kind = "retailer";
        shop.readPosts = {"/api/search", "*ksearchnet.com/cs/v2/search*"};
        shop.blocked = {"*remove=*"};
        shop.allowedPages = {"/account/orders"};

        SitePolicy amazon;
        amazon.id = "amazon";
        amazon.name = "Amazon";
        amazon.domains = {"amazon.ie"};
        amazon.kind = "marketplace";

        std::vector<SitePolicy> sites = {shop, amazon};
        std::map<std::string, SiteState> states = {
            {"shop", {ReadState::on, WriteState::ask}},
            {"amazon", {ReadState::on, WriteState::off}}
        };

        auto req = [](std::string method, std::string url, std::string resType = "Fetch", std::string pageUrl = "https://www.shop.ie/p/1") {
            GuardRequest r;
            r.method = std::move(method);
            r.url = std::move(url);
            r.resourceType = std::move(resType);
            r.pageUrl = std::move(pageUrl);
            return r;
        };

        CHECK(decide(req("GET", "chrome-extension://abc/inline/injected.js", "Script"), sites, states).action == "continue");
        CHECK(decide(req("GET", "data:image/png;base64,AA", "Image"), sites, states).action == "continue");
        CHECK(decide(req("GET", "https://www.shop.ie/p/2"), sites, states).action == "continue");
        CHECK(decide(req("GET", "https://www.shop.ie/basket", "Document"), sites, states).action == "continue");
        CHECK(decide(req("GET", "https://cdn.other.com/app.js", "Script"), sites, states).action == "continue");
        CHECK(decide(req("POST", "https://www.shop.ie/basket/add"), sites, states).action == "ask");
        CHECK(decide(req("PUT", "https://api.shop.ie/cart"), sites, states).action == "ask");
        CHECK(decide(req("POST", "https://www.shop.ie/api/search?q=x"), sites, states).action == "continue");

        auto gqlQuery = req("POST", "https://www.shop.ie/api/search");
        gqlQuery.body = "{\"query\":\"{ products(search: \\\"x\\\") { total_count } }\"}";
        CHECK(decide(gqlQuery, sites, states).action == "continue");

        auto gqlMut = req("POST", "https://www.shop.ie/api/search");
        gqlMut.body = "{\"query\":\"mutation { addProductsToCart(cartId: \\\"1\\\") { cart { id } } }\"}";
        CHECK(decide(gqlMut, sites, states).action == "ask");

        auto batchMut = req("POST", "https://www.shop.ie/api/search");
        batchMut.body = "[{\"query\":\"{ a }\"},{\"query\":\" mutation M { b }\"}]";
        CHECK(decide(batchMut, sites, states).action == "ask");

        auto crossMut = req("POST", "https://eucs31v2.ksearchnet.com/cs/v2/search");
        crossMut.body = "mutation { x }";
        CHECK(decide(crossMut, sites, states).action == "fail");

        CHECK(decide(req("POST", "https://www.amazon.ie/cart/add", "Fetch", "https://www.amazon.ie/dp/B0"), sites, states).action == "fail");
        CHECK(decide(req("POST", "https://eucs31v2.ksearchnet.com/cs/v2/search"), sites, states).action == "continue");

        auto crossPost = req("POST", "https://eucs31v2.ksearchnet.com/cs/v2/search", "Fetch", "https://www.amazon.ie/s");
        auto crossDec = decide(crossPost, sites, states);
        CHECK(crossDec.action == "fail");
        CHECK(crossDec.quiet == true);

        auto analytics = req("POST", "https://analytics.example.com/collect");
        auto analyticsDec = decide(analytics, sites, states);
        CHECK(analyticsDec.action == "fail");
        CHECK(analyticsDec.quiet == true);

        CHECK(decide(req("GET", "https://www.shop.ie/checkout", "Document"), sites, states).action == "fail");
        CHECK(decide(req("GET", "https://www.amazon.ie/gp/buy/spc/handlers/display.html", "Document"), sites, states).action == "fail");
        CHECK(decide(req("GET", "https://www.paypal.com/checkoutnow?token=1", "Document"), sites, states).action == "fail");
        CHECK(decide(req("POST", "https://api.stripe.com/v1/payment_intents"), sites, states).action == "fail");
        CHECK(decide(req("GET", "https://www.shop.ie/account/password", "Document"), sites, states).action == "fail");
        CHECK(decide(req("GET", "https://www.shop.ie/basket?remove=1", "Document"), sites, states).action == "fail");
        CHECK(decide(req("GET", "https://www.amazon.ie/your-account", "Document"), sites, states).action == "fail");
        CHECK(decide(req("GET", "https://www.shop.ie/account/orders", "Document"), sites, states).action == "continue");

        auto iframeUnknown = req("GET", "https://chat.example.com/widget", "Document");
        iframeUnknown.isMainFrame = false;
        auto iframeDec = decide(iframeUnknown, sites, states);
        CHECK(iframeDec.action == "fail");
        CHECK(iframeDec.quiet == true);

        CHECK(decide(req("GET", "https://mail.google.com/mail/u/0", "Document"), sites, states).action == "fail");
    }
}

// ============================================================================
// 4. Pages Challenge Detection Tests (pages.test.ts)
// ============================================================================
TEST_SUITE("pages challenges") {
    TEST_CASE("isChallenge recognises bot-check pages and ignores ordinary ones") {
        CHECK(isChallenge("<div class=\"cf-turnstile\"></div>") == true);
        CHECK(isChallenge("<a href=\"https://www.google.com/sorry/index\">") == true);
        CHECK(isChallenge("<html><head><title>Human Verification</title></head><body>Let's confirm you are human. Complete the security check before continuing.</body></html>") == true);
        CHECK(isChallenge("<script>var captcha = false;</script><title>Cordless drill</title>") == false);
        CHECK(isChallenge("<title>Human hair wigs</title>") == false);
    }
}

// ============================================================================
// 5. Adapter Helpers & Registry Tests (adapter.test.ts)
// ============================================================================
TEST_SUITE("adapter helpers") {
    TEST_CASE("parsePrice & parseEuro & parseGbp") {
        CHECK(parsePrice("€ 60,95") == 60.95);
        CHECK(parsePrice("€1,299.00") == 1299.00);
        CHECK(parsePrice("36.95") == 36.95);
        CHECK(parsePrice("€ 26 .95 INC VAT") == 26.95);
        CHECK(parseEuro("€89.99") == 89.99);
        CHECK(parseGbp("£49.50") == 49.50);
        CHECK(parsePrice("not a price") == std::nullopt);
    }

    TEST_CASE("parseStock") {
        CHECK(parseStock("In stock") == true);
        CHECK(parseStock("Available") == true);
        CHECK(parseStock("5 in stock") == true);
        CHECK(parseStock("Out of stock") == false);
        CHECK(parseStock("Sold out") == false);
        CHECK(parseStock("discontinued") == false);
        CHECK(parseStock("unknown") == std::nullopt);
        CHECK(parseStock("") == std::nullopt);
    }

    TEST_CASE("parseQuantity") {
        auto q1 = parseQuantity("Box of 100");
        REQUIRE(q1.has_value());
        CHECK(q1->count == 100);

        auto q2 = parseQuantity("100 Pack");
        REQUIRE(q2.has_value());
        CHECK(q2->count == 100);

        auto q3 = parseQuantity("6-pack");
        REQUIRE(q3.has_value());
        CHECK(q3->count == 6);

        auto q4 = parseQuantity("5 x 80");
        CHECK(!q4.has_value());
    }

    TEST_CASE("canonicalUrl") {
        CHECK(canonicalUrl("https://shop.ie/p/1?utm_source=google&gclid=123", "https://shop.ie/") == "https://shop.ie/p/1");
        CHECK(canonicalUrl("/p/2?keep=1&utm_medium=cpc", "https://shop.ie/", {"keep"}) == "https://shop.ie/p/2?keep=1");
        CHECK(canonicalUrl("/p/3#review-section", "https://shop.ie/") == "https://shop.ie/p/3");
    }

    TEST_CASE("distanceKm: straight-line haversine") {
        // Dublin to Cork (~219 km)
        double d = distanceKm(53.3498, -6.2603, 51.8985, -8.4756);
        CHECK(d >= 218.0);
        CHECK(d <= 220.0);
    }

    TEST_CASE("capDetails") {
        Offer offer;
        offer.details["k1"] = "v1";
        offer.details["k2"] = std::string(300, 'y');
        for (int i = 3; i <= 12; ++i) {
            offer.details["k" + std::to_string(i)] = "v" + std::to_string(i);
        }
        capDetails(offer);
        CHECK(offer.details.size() == 8);
        CHECK(offer.detailsOmitted == 4);
        CHECK(offer.details["k2"].get<std::string>().size() <= 203); // 200 chars + 3 byte ellipsis
    }

    TEST_CASE("fitToBudget") {
        std::vector<Offer> offers;
        for (int i = 0; i < 20; ++i) {
            Offer o;
            o.site = "siteA";
            o.id = std::to_string(i);
            o.name = "Product " + std::to_string(i);
            o.price = Price{10.0, "EUR", std::nullopt, true};
            offers.push_back(std::move(o));
        }
        auto res = fitToBudget(offers, 1000);
        CHECK(res.offers.size() < 20);
        CHECK(res.offers.size() >= 1);
        CHECK(res.trimmed["siteA"] > 0);
    }

    TEST_CASE("applyFilters") {
        std::vector<Offer> offers;
        for (int i = 1; i <= 10; ++i) {
            Offer o;
            o.site = "shop";
            o.id = std::to_string(i);
            o.name = (i % 2 == 0) ? "Bosch Drill" : "Makita Saw";
            o.brand = (i % 2 == 0) ? "Bosch" : "Makita";
            o.price = Price{static_cast<double>(i * 10), "EUR", std::nullopt, true};
            o.inStock = (i != 5);
            if (i == 4) {
                o.quantity = Quantity{2, "pack", true};
            }
            offers.push_back(std::move(o));
        }

        SearchFilters filters;
        filters.minPrice = 20.0;
        filters.maxPrice = 80.0;
        filters.inStock = true;
        filters.brand = "bosch";

        auto res = applyFilters(offers, filters, {}, "EUR");
        for (const auto& o : res.offers) {
            CHECK(o.price.amount >= 20.0);
            CHECK(o.price.amount <= 80.0);
            CHECK(o.inStock != false);
            CHECK(o.brand == "Bosch");
        }
        // Check unit price calculation
        for (const auto& o : res.offers) {
            if (o.id == "4") {
                CHECK(o.unitPrice == 20.0); // 40 / 2
            }
        }
    }

    TEST_CASE("blockedPostsHint") {
        auto store = Store::open(":memory:");
        GuardEvent ev1;
        ev1.at = 100;
        ev1.pageId = -1;
        ev1.site = "screwfix";
        ev1.method = "POST";
        ev1.url = "https://abc.awswaf.com/abc/verify?token=1";
        ev1.action = "fail";
        ev1.outcome = "blocked";
        ev1.reason = "cross-site POST blocked";
        store.logGuard(ev1);

        GuardEvent ev2;
        ev2.at = 101;
        ev2.pageId = -1;
        ev2.site = std::nullopt;
        ev2.method = "POST";
        ev2.url = "https://www.google-analytics.com/g/collect";
        ev2.action = "fail";
        ev2.outcome = "blocked";
        ev2.reason = "cross-site POST blocked";
        store.logGuard(ev2);

        std::string hint = blockedPostsHint(store, -1, 50);
        CHECK(hint.find("abc.awswaf.com/abc/verify") != std::string::npos);
        CHECK(hint.find("google-analytics") == std::string::npos);

        store.close();
    }
}
