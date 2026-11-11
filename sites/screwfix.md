---
id: screwfix
name: Screwfix Ireland
domains: [screwfix.ie]
kind: retailer
categories: [home-improvement]
urls:
  search: https://www.screwfix.ie/search?search={query}
  check-search: https://www.screwfix.ie/search?search=cordless+drill
  check-product: https://www.screwfix.ie/p/dewalt-dcd778m2t-sfgb-18v-2-x-4-0ah-li-ion-xr-brushless-cordless-combi-drill/979hf
  stores: https://www.screwfix.ie/stores/all
  store: https://www.screwfix.ie/stores/{branchCode}/{slug}
  delivery: https://www.screwfix.ie/help/delivery
  click-and-collect: https://www.screwfix.ie/help/clickandcollect
  returns: https://www.screwfix.ie/help/returns
defaults:
  read: on
  write: ask
readPosts:
  # AWS WAF silent JS challenge (not a CAPTCHA): the page refreshes its aws-waf-token here; without it every request gets 202 "challenge".
  - "*.awswaf.com/*/verify*"
  - "*.awswaf.com/*/mp_verify*"
blocked:
  - /my-account
  - "my.screwfix.ie/*"
  - "*/prod/ffx-browse-bff/*/basket*"
allowedPages: []
concurrency: 2
---

# Screwfix Ireland (screwfix.ie)

Next.js (Pages router) on a Bloomreach (BrXM/Hippo) headless CMS and Bloomreach discovery. Currency EUR. All page data is server-rendered into `__NEXT_DATA__`; there is no JSON search API to call. AWS WAF runs a JS challenge in browsers, but plain HTTP with a browser User-Agent works (the `page` tool does this). Without a UA the `fetch` CLI gets 403.

## URL patterns

- Search: `https://www.screwfix.ie/search?search={query}` (use `+` or `%20`). Broad queries often 302 to a category page, e.g. "cordless drill" goes to `/c/tools/drills/cat830704?powersupply=cordless&cm_sp=managedredirect-_-powertools-_-cordlessdrills` and "pocket hole jig" goes to `/c/tools/router-jigs/cat5990028`. Both are the same listing structure, so treat the final URL as the results page.
- Category: `/c/<dept>/<slug>/cat<digits>`. Refinements are query params named after `filters[].filterIdentifier` (`brand`, `price`, `starrating`, `powersupply`...). Each filter option has a ready `seoUrl` in `filters[].options[]`. Page size is 20: `pagination = {offset, limit}`; use the page's pagination links (`?page=2`).
- Product: `/p/<slug>/<sku>`, sku lower-case (e.g. `979hf`). The slug is not checked; the sku is what resolves.
- Store finder: `/stores` (map UI) and `/stores/all` (every store, with `branchCode`). Store page: `/stores/<branchCode>/<slug>`, e.g. `/stores/CI4/dublin-kylemore-road`.
- Reviews: Bazaarvoice (`screwfix-emea`), read by the adapter's `reviews()`: `GET https://apps.bazaarvoice.com/bfd/v1/clients/screwfix-emea/api-products/cv2/resources/data/reviews.json?resource=reviews&action=REVIEWS_N_STATS&filter=productid:eq:<SKU>&sort=rating:asc&limit=…&apiversion=5.5&displaycode=22465-en_ie` with header `bv-bfd-token: 22465,main_site,en_IE` (public display code). `sort=rating:asc` gives critical reviews first. CORS allows the screwfix.ie origin.
- Help: `/help/delivery`, `/help/clickandcollect`, `/help/returns`.

## Extraction hints

Parse `<script id="__NEXT_DATA__" type="application/json">`. Data lives in `props.pageProps.page.page.<uid>.models.enrichedData`. The `<uid>` keys are random per page, so find the component by `name`:

- Listing (search and category): component `name === "product-listing"`. `enrichedData.products[]`, `totalProducts`, `pagination`, `filters[]`, `breadcrumbs`, `store`.
- Product: component `name === "product-details"`. `enrichedData.productDetails`, `breadcrumbs`.
- Header component `name === "header"`: `enrichedData.store` is the selected store (empty `id`/`name` when none), `customer` (never read).
- Product JSON-LD (`application/ld+json`, `@type: Product`): `sku`, `offers`, `aggregateRating`.

Fields per product row (listing and `productDetails`): `skuId`, `longDescription` (title), `detailPageUrl`, `brand`, `starRating`, `numberOfReviews`, `bullets[]`, `priceInformation.currentPriceIncVat.amount`, `currentPriceExVat.amount`, `wasPriceIncVat.amount`, `savingsPercent`, `vatRate` (0.23), `isBulkPricingAvailable`, `fulfilmentAvailability.{canBeDelivered,canBeCollected}`, `collectionOnly`, `stockedInTc`, `promotion`, `productAvailabilityStatus`. Product details add `technicalSpecifications[{name,value}]` (Model No, Manufacturer Guarantee, Condition), `overview`, `images[]`.

Copyable `evaluate` recipe (read-only), listing rows or product details on the current page:

```js
() => {
  const d = JSON.parse(document.getElementById("__NEXT_DATA__").textContent);
  const comps = Object.values(d.props.pageProps.page.page);
  const find = (n) => comps.find((c) => c.name === n)?.models?.enrichedData;
  const l = find("product-listing");
  if (l) return { total: l.totalProducts, store: l.store, rows: l.products.map((p) => ({
    sku: p.skuId, title: p.longDescription, url: "https://www.screwfix.ie" + p.detailPageUrl,
    price: p.priceInformation?.currentPriceIncVat?.amount, was: p.priceInformation?.wasPriceIncVat?.amount,
    rating: p.starRating, reviews: p.numberOfReviews, stock: p.productAvailabilityStatus,
    canDeliver: p.fulfilmentAvailability?.canBeDelivered, canCollect: p.fulfilmentAvailability?.canBeCollected })) };
  const p = find("product-details")?.productDetails;
  return p && { sku: p.skuId, title: p.longDescription, price: p.priceInformation?.currentPriceIncVat?.amount,
    was: p.priceInformation?.wasPriceIncVat?.amount, stock: p.productAvailabilityStatus,
    specs: p.technicalSpecifications };
}
```

The same JSON can be pulled from a plain HTTP response with a regex on the `__NEXT_DATA__` script tag.

## Price format

- Rendered text is `€ 26 .95 INC VAT` (euros and cents in separate spans). Use the JSON `amount` (a number) instead of scraping text.
- Prices default to inc-VAT (23%). `currentPriceExVat` is also present; the site can toggle ex-VAT (`excludeVat`), which is a state change, so don't.
- `wasPrice*` and `savingsPercent` are only present on reductions. Bulk-save tiers show in a popup, not in the JSON row.

## Stock

- Adapter: `sites/screwfix.ts`. `search`/`offer` read `__NEXT_DATA__`; `pickup` uses the stock search API below.
- Per-branch stock by coordinates, no store selection needed (found 2026-10-15 from the "Check stock in your local store" page, `/checkstock?product_id=<sku>&quantity=1`): `GET /prod/ffx-browse-bff/v1/SFXIE/stock/search?productId=<sku>&lat=<lat>&long=<lon>&quantity=1` with header `authorization: <tykToken>`. The token is public page config (`"tykToken"` in `__NEXT_DATA__`), the same for every visitor; read it from the page each time, never copy it into notes. Without it the API answers "Invalid token".
- Response `nearestStores[]` (about 8, nearest first): `storeInfo.store.basic.{branchCode,name}`, `storeInfo.directDistance` (metres, straight line), `carRoute.durationText`, `products[].{branchStock, productAvailabilityStatus, collectionPromiseDate}`. `branchStock` is the shelf quantity.
- `productAvailabilityStatus` in listings is relative to the user's selected store: `MISSING_SELECTED_STORE` (none), `AVAILABLE_TO_COLLECT_TODAY`, `AVAILABLE_TO_COLLECT_NEXT_DAY`, `NOT_AVAILABLE_TO_COLLECT`, `ONLY_AVAILABLE_...` (delivery only).
- `collectionOnly: true` means not deliverable.

## Stores near Dublin

Hours for all: Mon-Fri 07:00-20:00, Sat 07:00-18:00, Sun 10:00-18:00. Freephone 1800 946 602. Source: store pages (`/stores/<code>/<slug>`).

| Branch | Store | Address | Lat, lon |
|---|---|---|---|
| CI4 | Dublin - Kylemore Road | Unit 3 Westlink Industrial Estate, Kylemore Road, Dublin 10, D10 CY26 | 53.337111, -6.346349 |
| GI8 | Dublin - Talbot Street | Unit 1 Talbot Mall, Talbot Street, Dublin, D01 N2W2 | 53.350391, -6.255736 |
| BI5 | Dublin - Glasnevin | Unit 103A Barrow Road, Dublin Industrial Estate, Dublin 11, D11 HX21 | 53.374840, -6.292255 |
| AI3 | Dublin - Finglas | Unit 41 North Park, North Road, Dublin 11, D11 PF57 | 53.400925, -6.313816 |
| CI6 | Dublin - Ballymount | Unit 2 Ballymount Road Lower, Walkinstown, Dublin 12, D12 DFP5 | 53.316954, -6.342952 |
| BI3 | Dublin - Darndale | Unit 1B Malahide Road Industrial Park, Newtown Avenue, Dublin 17, D17 YX58 | 53.396616, -6.194133 |
| AI1 | Swords | Unit 4A Feltrim Business Park, Mountgorry Way, Swords, K67 AY90 | 53.448244, -6.205938 |
| AI7 | Dublin - Sandyford | Unit 24 Spruce Avenue, Stillorgan Industrial Park, Dublin 18, A94 V2X8 | 53.280176, -6.220016 |
| CI9 | Naas | Unit A1 Crabapple Road, Naas Enterprise Park, Naas, W91 Y4A6 | 53.206218, -6.741727 |
| DI6 | Ashbourne | Unit 3 Ashbourne Retail Park, Ballybin Road, Ashbourne, A84 V672 | 53.522271, -6.415672 |
| GI6 | Bray | 27 Beechwood Close, Bray, A98 AE03 | 53.187756, -6.105440 |

Near Maynooth (53.381, -6.591): Finglas, Glasnevin, Kylemore Road, Ballymount, Naas, Ashbourne are within about 25 km. Sandyford and Bray are far south Dublin.

## Delivery and collection

Source: https://www.screwfix.ie/help/delivery , https://www.screwfix.ie/help/clickandcollect
- Delivery to store, site or home: EUR 5.00, free over EUR 50.00 (site message: "FREE weekday Delivery on orders over €50"). "Orders take a minimum of 2-3 working days to arrive as items are sent from our distribution centre." Signature required; delivery day selectable up to 7 days ahead.
- Some items are delivered by approved suppliers (DSV) with restrictions: https://www.screwfix.ie/help/delivery#delivery-restrictions-supplier .
- Click & Collect: "check stock in your local store, pay online and collect ... in as little as 1 minute". Wait for the "Ready to collect" SMS. Items not in the store arrive in 2-3 working days. Bring order number and the payment card (photo ID for PayPal or Apple/Google Pay).

## Returns and warranty

Source: https://www.screwfix.ie/help/returns
- 30-day money-back on unwanted items if unused, saleable and in original packaging. Excluded: cut, mixed or made-to-measure goods, used macerators, toilets, water pumps, opened boilers.
- Faulty within 30 days: refund or replacement. Later: repair, replacement or price reduction under consumer law (manufacturer warranty first where offered). Power tool repair service via stores.
- Refunds: cards 3-5 working days, PayPal 5-10.
- Manufacturer guarantee is in `technicalSpecifications` ("Manufacturer Guarantee").

## Price match

No Irish price-match page found (`/help/price-match` is 404, `/help/price-promise` is 403). Treat as unknown; don't claim one.

## Quirks

- Plain HTTP needs a browser User-Agent; AWS WAF may challenge unusual clients. Stop on a CAPTCHA.
- `__NEXT_DATA__` includes `userIpAddress` and `cookies`. Never copy these into notes.
- Header `customer` data appears when signed in. Never read or report it.
- A user's selected store changes `productAvailabilityStatus` and which store shows in `store`. Report the store alongside any stock claim.
- Pages of 20 rows; broad queries land on a category with facets rather than a keyword result list. Refine with a more specific query or facet params.
