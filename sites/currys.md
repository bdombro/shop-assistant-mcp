---
id: currys
name: Currys Ireland
domains: [currys.ie]
kind: retailer
categories: [electronics]
urls:
  search: https://www.currys.ie/search?q={query}
  check-search: https://www.currys.ie/search?q=laptop
  check-product: https://www.currys.ie/products/asus-vivobook-14-x1404ma-14-laptop-intel-core-5-256-gb-ssd-blue-10305861.html
  stores: https://www.currys.ie/store-finder
  delivery: https://www.currys.ie/services/delivery-installation/delivery-collection.html
  returns: https://www.currys.ie/services/shopping-with-us/returns-refunds.html
  price-promise: https://www.currys.ie/help-and-services/price-promise.html
defaults:
  read: on
  write: ask
readPosts:
  # Store stock by coordinates (a read, despite POST); the page sends its own csrf token.
  - /product-stores-collection
blocked:
  - /basket
  - /cart*
  - /checkout*
  - /login*
  - /account*
  - /on/demandware.store/*Cart*
  - /on/demandware.store/*Checkout*
  - /on/demandware.store/*Account*
  - /on/demandware.store/*Login*
allowedPages: []
stores:
  - { id: "2968", name: Jervis Centre, lat: 53.347752, lon: -6.265185 }
  - { id: "2926", name: Liffey Valley, lat: 53.348348, lon: -6.391554 }
  - { id: "2975", name: Blanchardstown, lat: 53.393585, lon: -6.391736 }
  - { id: "2970", name: Swords, lat: 53.447235, lon: -6.224924 }
  - { id: "2969", name: Carrickmines, lat: 53.25052, lon: -6.1849 }
  - { id: "2979", name: Naas, lat: 53.20986, lon: -6.70187 }
  - { id: "2933", name: Navan, lat: 53.655843, lon: -6.701746 }
  - { id: "2948", name: Dundalk, lat: 53.988408, lon: -6.377692 }
  - { id: "2950", name: Carlow, lat: 52.846301, lon: -6.902391 }
concurrency: 2
---

# Currys Ireland (currys.ie)

Salesforce Commerce Cloud (SFCC/Demandware; assets under `/on/demandware.static/Sites-curryspcworldie-Site/`) behind Cloudflare. Currency EUR, prices inc VAT. **Plain HTTP is blocked**: `curl` and `fetch` get 403 "Attention Required! | Cloudflare" even with a full Chrome User-Agent and Accept headers. Use the browser transport for every page kind. A Turnstile script (`turnstile.js`) is present; stop on any challenge. In the user's real browser the pages load normally.

## URL patterns

- Search: `https://www.currys.ie/search?q={query}`. Broad queries 302 to a category listing with `?searchTerm=`, e.g. "laptop" goes to `/computing/laptops/laptops?searchTerm=laptop`. Same markup either way.
- Paging: `?start=20&sz=20` (20 rows per page; `start` is the offset). Sort and facets are links in the left rail (query params `prefn1/prefv1` SFCC style; read them from the page links).
- Product: `/products/<slug>-<8-digit-id>.html` (e.g. `...-10305861.html`). The id is the product id (`data-pid`), not the SKU. EAN is in the JSON-LD `gtin13`.
- Reviews: on the product page (`#pdp-reviews`), loaded client-side. Aggregate in the tile and JSON-LD `aggregateRating`.
- Store finder: `/store-finder`; store pages `/store-finder/<slug>-<id>.html`. (`/stores` is a 404.)
- Help: delivery, returns, price promise URLs in frontmatter.

## Adapter

`sites/currys.ts`: search, offer, pickup, reviews. Search and product read the HTML through in-page fetch (listing tiles carry a JSON datalayer with price, was-price, EAN and online stock status; the product page has JSON-LD and spec tables). `gtin` is the EAN. Pickup posts `/product-stores-collection` (coordinates, product id, page csrf token) and reads per-store counts (`ats`) for the stores below. Reviews are Feefo (merchant `currys-ie`, `GET api.feefo.com/api/10/reviews/product`), pooled with Currys UK and other merchants (marked syndicated); many ratings have no text and are skipped. Page text for other pages (category browsing, help) still comes from `page`.

Price notes: `baseRevenue` is the pre-drop price, not an RRP. "+N more offers" are mostly bundle promos (accessory discounts), not price changes. Product pages also show accessory tiles with their own ratings; don't mistake those for the product's reviews. Online stock is a status (`IN_STOCK`), not a count.

## Stores near Dublin

Source: https://www.currys.ie/store-finder (coordinates from the page's `data-locations`; addresses from store cards). Hours shown on cards: Jervis 09:00-18:30, others 09:00-20:00. Phone (customer service) 0818 440 001. Distances: from Maynooth (53.381,-6.591) and Dublin centre (53.349,-6.260). Frontmatter `stores` also lists Naas, Navan, Dundalk and Carlow (from the stock response).

| Store | Address | Lat, lon | Maynooth | Centre |
|---|---|---|---|---|
| Jervis Centre | Unit 13-15 Jervis Shopping Centre, Dublin D01 E7W2 | 53.347752, -6.265185 | ~23 km | ~0.4 km |
| Liffey Valley | Unit 15 Liffey Valley Retail Park, Clondalkin D22 E789 | 53.348348, -6.391554 | ~16 km | ~9 km |
| Blanchardstown | Unit 407 Blanchardstown Retail Park, D15 CK6X | 53.393585, -6.391736 | ~14 km | ~9 km |
| Swords | Unit 11 Airside Retail Park, Swords K67 FY06 | 53.447235, -6.224924 | ~35 km | ~12 km |
| Carrickmines | Unit 9 The Park, Carrickmines D18 DP23 | 53.250520, -6.184900 | ~42 km | ~12 km |

Nearest to Maynooth: Blanchardstown, then Liffey Valley. (Distances are rough straight-line estimates.)

## Delivery and collection

Source: https://www.currys.ie/services/delivery-installation/delivery-collection.html
- Small items: from EUR 6.99, 3-5 working days, Mon-Fri. Large items (major appliances, TVs over 43"): EUR 39.99 (range cookers and American fridge freezers EUR 49.99), 5-7 working days, Tue-Sat, 4-hour window by text 2 days before.
- Promos change: the site banner said "Free delivery on laptops" and Windows laptops had a "Free delivery" offer. Check the product's offers.
- Free in-store collection, subject to store availability. No Parcel Motel delivery. No customs charges.

## Returns and warranty

Source: https://www.currys.ie/services/shopping-with-us/returns-refunds.html
- Faulty within 30 days: exchange or refund. Within the guarantee period (normally 12 months): repair service. Contact 0818 440 001.
- Changed mind: online purchases returnable within 30 days even if opened (as new, original packaging where possible); in-store purchases within 30 days unopened and sealed with proof of purchase. Small items are collected from home through a returns form. Hygiene exclusions and connected products with airtime apply.

## Price match

Source: https://www.currys.ie/help-and-services/price-promise.html
"We promise to match or beat the price you'd pay at selected Irish tech retailers, including Amazon.ie, and up to 7 days after you've bought from us." Matches DID Electrical, Harvey Norman, Power City and Amazon.ie. Item must be brand new, identical, same terms, both in stock. Price only, not delivery. Excludes membership-only prices; can't combine with other offers. Request by phone (0818 440 001), email, or in store, with the product code, price and competitor.

## Quirks

- Cloudflare blocks all non-browser HTTP. `fetch`, `probe` and `page` over http will return the block page.
- Search often redirects to a category with facets.
- Footer lists Currys UK, Currys Business, Partmaster: other domains, not covered here.
