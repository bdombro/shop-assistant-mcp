---
id: brooks
name: Brooks
domains: [brooksonline.ie]
kind: retailer
categories: [home-improvement]
urls:
  search: "https://brooksonline.ie/catalogsearch/result/?q={query}"
  check-search: "https://brooksonline.ie/catalogsearch/result/?q=hammer+drill"
  check-product: "https://brooksonline.ie/reisser-powerbohr-sds-plus-hammer-drill-bit-25-0-x-1000mm-up3754"
  branches: "https://brooksonline.ie/branch-locator/"
  delivery: "https://brooksonline.ie/delivery-information"
  collect: "https://brooksonline.ie/click-and-collect"
  returns: "https://brooksonline.ie/returns-and-refunds"
  terms: "https://brooksonline.ie/terms-conditions"
defaults: { read: on, write: ask }
blocked: ["/checkout", "/customer/account", "/wishlist", "/sendfriend", "/get-a-quote", "*/checkout*", "*/customer/account*", "*/wishlist*", "/assign_postcode"]
concurrency: 2
---
Brooks (brooksonline.ie), Irish builders' merchant and timber supplier (Magento 2, Cloudflare). Prices are EUR; shown prices include VAT and the ex-VAT price sits in the same box. Trade prices are behind a login: show retail prices and say so. Online prices apply to online orders only. Republic of Ireland only.

Adapter `sites/brooks.ts`: search, offer (id = product code like `MBMB88480`; a search by code finds it) and pickup. Pickup reads the branch list (names, coordinates, per-branch stock) embedded in each product page: no postcode, no POST, no `stores` list needed. "In stock" means on the shelf at the branch now; click and collect is usually ready within two hours of opening. No reviews (the review widget is third-party and empty on most items).

## URL patterns

- Search: `/catalogsearch/result/?q={query}`, 24 per page, `&p=2` for more. A query can redirect to a category listing with the same row markup. Matching is loose; "Related search terms" appear above the grid.
- Product: `/<slug>` with no `.html`. Category: `/building-material/<group>/<subgroup>`.
- Cart, checkout, account, quote forms and wishlist are blocked.

## Delivery and returns

Sources: `/delivery-information`, `/click-and-collect`, `/returns-and-refunds`.

- Orders dispatch in 2-4 working days (product pages show their own "In N working days"). Free delivery from €250; under €250 €50 (heavy) or €12 (small items). Heavy and super-heavy items only go to listed Eircode zones (Dublin, Meath, Kildare, Wicklow, Cork city, Galway, Sligo and others); outside them, email for a quote.
- Click & Collect is free, usually ready within two hours in opening hours; items showing "several days" are ordered in. Hold 7 days.
- Returns: 28 days, unopened and resalable, buyer pays return postage unless faulty. Restocking charge 20% flooring and doors, 25% Velux, 15% non-stocked items (5+ day lead time); bagged powders can't be returned.
