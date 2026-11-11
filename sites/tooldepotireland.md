---
id: tooldepotireland
name: Tool Depot Ireland
domains: [tooldepotireland.ie]
kind: retailer
categories: [home-improvement]
urls:
  search: https://www.tooldepotireland.ie/wp-json/wc/store/v1/products?search={query}&per_page=20
  product: https://www.tooldepotireland.ie/wp-json/wc/store/v1/products/{id}
  product-page: https://www.tooldepotireland.ie/product/{slug}/
  check-search: https://www.tooldepotireland.ie/wp-json/wc/store/v1/products?search=drill&per_page=5
  check-product: https://www.tooldepotireland.ie/wp-json/wc/store/v1/products/31448
  returns: https://www.tooldepotireland.ie/refund_returns/
  terms: https://www.tooldepotireland.ie/terms-and-conditions/
  contact: https://www.tooldepotireland.ie/contact-us/
defaults:
  read: on
  write: ask
readPosts: []
blocked:
  - /cart
  - /checkout
  - /my-account
  - /wp-admin
  - /wp-login.php
  - /wishlist
  - /wp-json/wc/store/v1/cart
  - /wp-json/wc/store/v1/checkout
  - /wp-json/wc/store/v1/batch
allowedPages: []
stores:
  - id: wexford
    name: Tool Depot Wexford
    lat: 52.3053
    lon: -6.4675
concurrency: 2
---

# Tool Depot Ireland (tooldepotireland.ie)

WooCommerce with the Store API open to anonymous GET requests. Irish tool retailer based in Wexford (Drinagh, Rosemount Business Park). Currency EUR, VAT included in displayed prices. Ships nationwide next-day via DPD, and offers click and collect at the Wexford store. Plain HTTP with a browser User-Agent works for everything below; no browser needed.

## URL patterns

- Search: `https://www.tooldepotireland.ie/wp-json/wc/store/v1/products?search={query}&per_page=20`. Returns a JSON array (not an object). Paging: `&page=2`. Sort: `&orderby=price&order=asc` (standard WooCommerce Store API params). Category filter: `&category={term_id}`, ids from `/wp-json/wc/store/v1/products/categories`.
- Product by id: `/wp-json/wc/store/v1/products/{id}`. The id comes from the search row.
- Product page (human): `/product/{slug}/`. Slug is in each row.
- Shop and categories: `/shop/`, `/product-category/{slug}/`.
- Returns: `/refund_returns/`. Terms: `/terms-and-conditions/`. Contact and store hours: `/contact-us/`, `/about-us/`.
- Blocked: `/cart`, `/checkout`, `/my-account`, `/wishlist`, and every `wp-json/wc/store/v1/cart|checkout|batch` route. These create or change a session.

## Adapter

`sites/tooldepotireland.ts` supports search, offer and pickup (Wexford). No reviews: the site has no review service (`review_count` is 0; it links only Facebook and Instagram). Search needs few terms: matches names only: one or two words or a model number; extra words return `[]` ("makita drill" returns nothing, "makita combi" works).

Row quirks: prices are minor-unit strings, VAT included. Stock text: empty = on the shelf, `N in stock`, `Available to order – delivered in 5–6 working days` (supplier lead time, not on the shelf; Festool items), `Out of stock`. Stock is not per store. Festool specs are a `festool-specs` table in `description`.

## Policy

- Delivery: next-day anywhere in Ireland, via DPD. Source: `/about-us/` ("with our delivery partners at DPD we offer next-day delivery anywhere in Ireland"), `/` ("Next-day delivery or click & collect in Wexford"), and the FAQ on `/about-us/`.
- Delivery cutoff time: not found on any public page checked (home, about, shop, product, terms, returns). Recorded as unknown.
- Free-delivery threshold and delivery fee: not found on public pages. Shipping rates only show in cart and checkout, which are blocked. Recorded as unknown.
- Click and collect: Wexford store (Tool Depot, Unit 6, Sinnottstown Lane, Rosemount Business Park, Drinagh, Co. Wexford). Lat/lon from the store Google Maps link on product pages (52.3053, -6.4675). Opening hours (source `/about-us/`): Mon–Thu 8:30–17:30, Fri 8:30–16:30, Sat 9:00–13:00, Sun closed.
- Returns: 14 days from receipt, unused and in sellable condition, customer pays return carriage; refund of goods; uplift can be arranged for an extra charge. Source: `/refund_returns/`. Faulty goods: replacement or full refund including carriage. Warranty handled by the retailer on the customer's behalf (per `/refund_returns/`).
- Price match: not found.
- Stock: "Available to order" rows mean supplier lead time (5–6 working days) and not on the shelf. Stock is not per store.
- Seller: sold by Tool Depot Ireland directly (no marketplace seller in the Store API rows checked).
- Reviews: none.

## Quirks and risks

- Store API routes under `/wp-json/wc/store/v1/cart` set a session cookie and modify cart state. They are blocked. Do not call them.
- Nonce-protected routes (`/wp-json/wc/store/v1/batch`) are blocked too.
- Cookie banner (Breakdance/GTM) appears on HTML pages. Not relevant to the JSON routes.
- Homepage HTML is ~8 KB of text after cleanup. Promos (e.g. Oktoberfest event 15 Oct 2026) are noise.
- The Store API returns `404` on the old `/delivery/`, `/shipping/` slugs. Use `/about-us/` and `/refund_returns/`.

## Fixtures

- `fixtures/tooldepotireland/search.json` (search `drill`, 5 rows)
- `fixtures/tooldepotireland/product.json` (product 31448)
