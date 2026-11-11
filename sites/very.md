---
id: very
name: Very Ireland
domains: [very.ie]
kind: retailer
categories: [everything]
urls:
  search: https://www.very.ie/e/q/{query}.end
  check-search: https://www.very.ie/e/q/headphones.end
  check-product: https://www.very.ie/jbl-quantum-100-headphones-blacknbsp/1601251165.prd
  delivery: https://www.very.ie/web/en/free-delivery-returns.page
  help: https://www.very.ie/help/en/online-help-system
  terms: https://www.very.ie/help/en/terms-and-conditions.page
defaults:
  read: on
  write: ask
readPosts: []
blocked:
  - /basket.page
  - /checkout
  - /account
  - /saveditems.page
  - "*/accountsummary*"
concurrency: 2
---

# Very Ireland (very.ie)

Irish arm of The Very Group (Shop Direct Ireland Limited t/a Very), formerly Littlewoods Ireland. Verified: the CDN path is `media.very.ie/i/littlewoodsireland/...`, the Akamai rule is `Littlewoodsireland_VP_V1`, and `littlewoodsireland.ie` also answers (403 over HTTP). Currency EUR. Fashion, home, electricals, toys, gaming, beauty.

## Transport

Akamai Bot Manager returns 403 to every plain HTTP request, even with a Chrome UA and full browser headers (home, search, product, robots.txt, sitemap). Use the browser transport for everything. Brave loads it fine, no CAPTCHA seen.

## URL patterns

- Search: `https://www.very.ie/e/q/{query}.end` (hyphen or `%20` for spaces untested; single words verified). The site's own `/e/EC/search?q=` is a 404 page. Breadcrumb JSON-LD links to `/search/{query}`.
- Sort: `?sort=relevance|newin|price|pricedesc|highestrated|brand`. Page size 30 or 96. Facets (Department, Brand, Price Range, Customer Rating) are in the filter panel. Results header: "Results: 1 - 30 (of 162)".
- Product: `/<slug>/<digits>.prd` (the digits resolve). Item number (SKU, e.g. `WV9Y7`) and EAN are in the description text and JSON-LD.
- Category: `/browse/<slug>`, `/technology-gaming`, `/electricals/e/b/4739.end`.
- Delivery and returns: `/web/en/free-delivery-returns.page`. Help: `/help/en/online-help-system`.
- Reviews: Bazaarvoice (many are syndicated from very.co.uk).

## Adapter

`sites/very.ts`: search, offer, reviews (no pickup: online only, `deliveryOnly`). It reads the server-rendered search page and product JSON-LD by in-page fetch from a very.ie tab. Search rows carry the cash price, `was` and a "From" note for variant pricing; stock is only on `offer` (JSON-LD `availability`). Reviews come from Bazaarvoice (client `littlewoods-ie`, lowest rating first) through its browser proxy. `page` reads: product pages hold `__product_body_initial_state__` and a "Price matched" badge.

## Price format (cash vs credit)

Very pushes credit ("Very Pay", Take 3, spread the cost, "Credit provided subject to eligibility"). Search cards and the product JSON-LD show the plain cash price (e.g. `€24.99`). No weekly or monthly instalment amount was in the card or JSON-LD. Report the cash price and ignore instalment text. "From €X" means variants (sizes, colours) start at X; open the product for the exact price. EU price ends with no VAT note; "price you see at checkout is exactly what you'll pay" (no extra customs charges).

## Stock

Product page shows "In stock" and JSON-LD `availability`. No per-store stock (online only, no stores).

## Delivery, returns, price match

Source: https://www.very.ie/web/en/free-delivery-returns.page
- Standard (small items): EUR 3.99, usually 3-7 working days Mon-Fri. Large goods (beds, big TVs, appliances, sofas): EUR 30 per order, up to 8 working days or longer for supplier-direct. Estimated date shown at checkout.
- Returns: free under a 28-day approval guarantee (complete, unused, original packaging); free home collection or drop-off via the returns portal in My Account. Refund up to 14 days after receipt. Christmas extension: items bought 1 Oct to 10 Dec 2026 returnable until 8 Jan 2027. Some items are non-returnable once installed or connected (e.g. headphones in the sample listing: "Non returnable ... once installed or connected").
- Price match: product pages carry a "Price matched" badge. Policy page not read; treat details as unknown.
- No stores, no click and collect seen.

## Quirks

- Akamai blocks HTTP; stay at about one page load every few seconds.
- Login, basket and credit application are out of scope. "Basket", "Account", "Wish List" links in the header lead to `/basket.page`, `/account/...`, `/saveditems.page`.
- Cookie banner not shown in the Brave profile; never accept all.
