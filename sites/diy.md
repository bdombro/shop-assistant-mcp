---
id: diy
name: B&Q Ireland (diy.ie)
domains: [diy.ie]
kind: retailer
categories: [home-improvement]
urls:
  search: https://www.diy.ie/search?term={query}
  check-search: https://www.diy.ie/search?term=cordless+drill
  check-product: https://www.diy.ie/departments/black-decker-18v-li-ion-brushed-cordless-combi-drill-1-x-2ah-kfbcd701d1k/5035048700693_BQ.prd
  store: https://www.diy.ie/store/{storeId}
  delivery: https://www.diy.ie/customer-support/delivery
  click-and-collect: https://www.diy.ie/customer-support/click-collect
  returns: https://www.diy.ie/customer-support/returns-and-refunds
defaults:
  read: on
  write: ask
blocked:
  - /basket
  - /checkout
  - /customer
stores:
  - { id: "1259", name: "B&Q Dublin - Liffey Valley", lat: 53.3482, lon: -6.38797 }
  - { id: "1331", name: "B&Q Dublin - Tallaght", lat: 53.2935, lon: -6.372215 }
  - { id: "1337", name: "B&Q Dublin - Swords", lat: 53.4477, lon: -6.222108 }
  - { id: "3077", name: "B&Q Navan", lat: 53.650974, lon: -6.713451 }
  - { id: "1326", name: "B&Q Galway", lat: 53.272, lon: -9.099 }
  - { id: "3075", name: "B&Q Waterford", lat: 52.24374, lon: -7.084402 }
  - { id: "3076", name: "B&Q Letterkenny", lat: 54.949052, lon: -7.72924 }
concurrency: 2
---

# B&Q Ireland (diy.ie)

Kingfisher's React Router (SSR) storefront, EUR. The bare `fetch` CLI gets 503 without a browser User-Agent; with a Chrome UA, plain HTTP returns full server-rendered HTML (the `page` tool does this). Sister domain diy.com is the UK site: don't mix. robots.txt disallows `/search?*`, reviews tabs and `/basket`; the `page` tool is a user-driven read, but keep volume low.

## URL patterns

- Search: `https://www.diy.ie/search?term={query}` (`?q=` gives 400). 48 rows per page, `&page=2`. Broad terms stay on the search page with category tiles ("found it" grid) above the rows.
- Product: `/departments/<slug>/<ean>_BQ.prd` (the EAN/sku before `_BQ` is the id).
- Category: `/departments/<dept>/<sub>/DIY<digits>.cat`, also `/<slug>.cat` for clearance.
- Store page: `/store/<id>` (HardwareStore JSON-LD with geo and hours). Store finder `/find-a-store` is client-rendered (needs typing a location): don't use.
- Help: `/customer-support/delivery`, `/customer-support/click-collect`, `/customer-support/returns-and-refunds`.

## Extraction hints

- Search page has a JSON-LD `ItemList` (`itemListElement[]`: `name`, `url`, `sku`, `offers.price`, `offers.priceCurrency`, `aggregateRating.{ratingValue,reviewCount}`). Easiest source for rows.
- DOM: `[data-testid=product]` per row, with `product-name`, `product-link` (href), `product-price` (current), `was-price` (`<s>`, only on reductions), `savings`, `reviews-count`, `roundel` (e.g. "Offer").
- Product page JSON-LD `Product`: `brand`, `gtin13`, `offers[]` (one per fulfilment: "Click and Collect", "In-Store Purchase", home delivery, each with `availability` InStock/OutOfStock and `itemCondition`). The `data-testid=seller` element is empty for B&Q-sold items.
- Branch stock isn't in the page HTML; `availability` in the offers is the anonymous summary. Read it via the adapter: the site's `/browse-mfe/api/fulfilment-options?compositeOfferId=<ean>&storeId=<id>` (GET, no eircode; `inStore.quantity`). Reviews are Bazaarvoice (client `diy-ie`).

Adapter (`sites/diy.ts`): `search`, `offer`, `pickup` (per store id, one request each, quantities), `reviews` (lowest first). Home delivery is the site default (€6, free over €100).

## Price format

`€53.60`, `€73` (no decimals when whole). Was price shows as `Was €67`. Prices are inc-VAT. Per-unit prices (`€13 per l`) appear as separate text.

## Marketplace (third-party sellers)

B&Q runs a verified-sellers marketplace on diy.com, but on diy.ie the app config has `marketplaceFrontEndEnabled: false` and none of the sampled listings or products showed a seller. Strings in the bundle show how a marketplace item would look: PDP line "Sold & shipped by {seller}", delivery note "Shipped by a verified seller", CC note "Order online, collect from store" / "Available online only and not stocked in stores". `/verified-sellers/` is 404 here. Treat everything as sold by B&Q unless `data-testid=seller` has text; if it does, say it is a third-party seller and that returns and delivery terms may differ.

## Stores near Dublin

Source: store pages `https://www.diy.ie/store/<id>` (JSON-LD). Mon-Fri 07:00-20:00 unless noted. Only 8 stores in Ireland; frontmatter `stores` has 7 ids (also Galway 1326, Waterford 3075, Letterkenny 3076; the eighth wasn't found). Table below: Dublin area.

| Id | Store | Address | Lat, lon | Hours |
|---|---|---|---|---|
| 1259 | Dublin - Liffey Valley | Liffey Valley Retail Park East, Coldcut Road, Clondalkin, D22 E892 | 53.3482, -6.38797 | Sat 07-20, Sun 10-18 (verify) |
| 1331 | Dublin - Tallaght | Belgard Retail Park, Belgard Road, Tallaght, D24 W5RV | 53.2935, -6.372215 | Sat 07-20, Sun 10-18 |
| 1337 | Dublin - Swords | Airside Retail Park, Swords, K67 V3Y2 | 53.4477, -6.222108 | Sat 07-20, Sun 10-18 |
| 3077 | Navan | Unit 1, Navan Retail Park, Athboy Road, C15 A381 | 53.650974, -6.713451 | Mon-Fri 08-20, Sat 08-20, Sun 10-18 |

Liffey Valley is the closest to Maynooth (about 20 km) and to Dublin centre (about 8 km). The Liffey Valley store also holds some bulk-buy Click + Collect items. Other Irish stores (e.g. Galway 1326) are far.

## Delivery, collection, returns

- Delivery (https://www.diy.ie/customer-support/delivery): standard EUR 6, free over EUR 100; large items EUR 16, free over EUR 500; bulk delivery min order EUR 100 plus EUR 50 (Dublin area only). Mon-Sat 8am-6pm, you pick the date at checkout; standard orders can't be amended or cancelled.
- Click + Collect (https://www.diy.ie/customer-support/click-collect): free, ready in as little as 15 minutes (next day if ordered within an hour of closing). Bring the order QR/number; ID for age-restricted items. Uncollected after 7 days: auto refund.
- Returns (https://www.diy.ie/customer-support/returns-and-refunds): 90 days, original condition, receipt/order confirmation; any Irish store. Not for cut-to-size, made-to-measure, mixed items, perishables. Faulty: exchange, refund or repair.
- Guarantees: power tools have their own aftersales page linked from customer support (not verified).
- Price match: none found. `/prices-nailed-down` is a promo, not a match.

## Quirks

- 503 without a browser UA; no CAPTCHA seen over about 20 plain requests.
- `page` markdown for search has no product rows (only categories and a large "Filters" block); read the `jsonLd` ItemList in the `page` result for rows. Stripping filter selectors made it worse, so none is set.
- The page config embeds third-party API keys (maps); never copy them.
