---
id: mcquillantools
name: McQuillan Tools
domains: [mcquillantools.ie]
kind: retailer
categories: [home-improvement]
urls:
  search: "https://mcquillantools.ie/search.php?search_query={query}"
  check-search: "https://mcquillantools.ie/search.php?search_query=cordless+drill"
  check-product: "https://mcquillantools.ie/hikoki-dn18dsl-w4-cordless-angle-drill-body-only/"
  delivery: "https://mcquillantools.ie/delivery/"
  faqs: "https://mcquillantools.ie/faqs/"
  store-locator: "https://mcquillantools.ie/store-locator/"
  terms: "https://mcquillantools.ie/terms-conditions/"
defaults: { read: on, write: ask }
readPosts: []
blocked: ["/cart.php", "/checkout", "/login.php", "/account.php", "/orders", "/wishlist", "*/cart.php*", "*/checkout*", "*/login.php*", "*/account.php*"]
allowedPages: []
concurrency: 2
---

# McQuillan Tools (mcquillantools.ie)

Irish power tool and hand tool retailer, 34-36 Capel Street, Dublin D01 PX00. BigCommerce (Stencil theme) storefront with Doofinder hosted search. Currency EUR. Prices on the storefront are shown with VAT and ex-VAT side by side. Ships in Ireland. Also a wholesale branch in Cork (no coordinates recorded).

## URL patterns

- Search (HTML, used by the plugin): `https://mcquillantools.ie/search.php?search_query={query}` (use `+` for spaces). Results are 12 per page. Next page param not verified (try `?page=2`). Heading `N results for 'query'`.
- Product: `https://mcquillantools.ie/<slug>/` (no SKU in the URL). Category pages: `/<category-slug>/`.
- Store locator: `/store-locator/`. Delivery: `/delivery/`. FAQs: `/faqs/`. Terms: `/terms-conditions/`. Warranty for HiKoki: `/hikoki-warranty/`.
- Add-to-cart links are `cart.php?action=add&product_id=…`. Blocked, never follow.

## Adapter

`sites/mcquillantools.ts` supports search (Doofinder hosted search by GET) and offer (product page plus the Doofinder record for brand, GTIN, stock). No pickup (one shop, no stock shown: the page's "Current Stock" is empty and hidden) and no reviews (no review service). Delivery only.

## Page reads

- Product page: `h1.productView-title`, `input[name=product_id]` (same as Doofinder `id`), `data-product-price-with-tax` / `-without-tax` / `-non-sale-price-with-tax`, extra specs under `#accordion--custom-fields`. No JSON-LD `Product`, no SKU.
- Doofinder: store id `2c04ea28-48d0-4bf7-85c0-5b376442fb46`, hashid `f0cb44e7b3d3281a47899466fbd7699c` (public page config). It answers `request not authenticated` without an Origin of the site, so only call it from a site tab. `query` also matches product ids and model codes.

## Price format quirks

- Storefront: `€421.95` with a separate `(Ex. VAT)` / `(Inc. VAT)` line. Prices in the listing are inc VAT (23%) unless a column says otherwise.
- Doofinder `best_price` and `price` are inc VAT (checked on `cordless drill`: HiKoki DN18DSL body-only 259.0). HTML cards show `Inc. VAT` and `Ex. VAT` (ratio 1.23, e.g. €475.00 / €386.18).

## Stores with coordinates

- **Capel Street, Dublin** (McQuillan Tools, "Store Locator" entry: Capel Street Online Department). 34-36 Capel Street, Dublin D01 PX00, Ireland. Phone 018733944. Approx lat/lon **53.3478, -6.2651** (estimated from the street address, not geocoded: confirm before relying on it for distance). About 1 km from Dublin centre (53.349, -6.260); about 22 km from Maynooth (53.381, -6.591).
- **Cork Wholesale**, Capel Street, Cork (store-locator entry; no address or coordinates on the page). Not within range of Dublin. Lat/lon not recorded.

## Policy (source URLs)

- Delivery (source: `/faqs/`, `/delivery/`): under 30 kg €7.95 (per `/faqs/`; `/delivery/` says "anywhere in Ireland up to 30 kg" €7.95, 31-40 kg €20.00, over 40 kg €47.00). Free delivery in Ireland on sales over €75 (per `/faqs/`, "subject to change at anytime"). Gift card delivery €5.00. International via cart rates.
- Click and collect / in-store pickup: the order status list in `/faqs/` mentions "Awaiting Pickup" from a store location. Not verified how to choose it.
- Returns: `/faqs/` points to `/terms-conditions/#answer8`. Cancel before dispatch by email or contact form. Not read in full.
- HiKoki warranty: `/hikoki-warranty/`. Not read in full.
- Price match: not found.

## Quirks

- The UCP endpoint `https://www.mcquillantools.ie/.well-known/ucp` returns 200 but needs an API key. Not used.
- The `Add to Cart` button on search cards is a `cart.php` link. Blocked.
- A "large volume of queries" banner is on the contact page; that is store-side and not a block.
- Node 22 on this machine fails TLS (`UNABLE_TO_VERIFY_LEAF_SIGNATURE`) for `www.mcquillantools.ie`; the apex `mcquillantools.ie` works. Use the apex host in URLs.
