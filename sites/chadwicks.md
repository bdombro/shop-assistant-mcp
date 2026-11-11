---
id: chadwicks
name: Chadwicks
domains: [chadwicks.ie]
kind: retailer
categories: [home-improvement]
urls:
  search: https://www.chadwicks.ie/catalogsearch/result/?q={query}
  klevu-search: https://eucs31v2.ksearchnet.com/cs/v2/search
  check-search: https://www.chadwicks.ie/catalogsearch/result/?q=cordless+drill
  check-product: https://www.chadwicks.ie/dewalt-dck266p2-brushless-combi-impact-kit-53094.html
  stores: https://www.chadwicks.ie/stores/
  store: https://www.chadwicks.ie/stores/{slug}
  delivery: https://www.chadwicks.ie/deliveryfaq
  click-and-collect: https://www.chadwicks.ie/clickandcollectfaq
  terms: https://www.chadwicks.ie/terms-and-conditions
defaults: { read: on, write: ask }
blocked: ["/checkout", "/customer/account", "/sales/order", "/wishlist", "/cart", "*/checkout*", "*/customer/account*", "*/cart*"]
readPosts:
  - "*ksearchnet.com/cs/v2/search*"
  # Branch stock query (read-only GraphQL POST; GET is 403)
  - "*chadwicks.ie/graphql*"
stores:
  - id: "32"
    name: "Dublin 12, Robinhood Rd"
    lat: 53.321304
    lon: -6.349288
  - id: "80"
    name: "Wicklow, Bray"
    lat: 53.187892
    lon: -6.104491
  - id: "63"
    name: "Dublin 17, Coolock"
    lat: 53.394446
    lon: -6.194156
  - id: "79"
    name: "Dublin Co. - Donabate"
    lat: 53.492806
    lon: -6.193371
  - id: "81"
    name: "Dublin 3, East Wall Road"
    lat: 53.353134
    lon: -6.226143
  - id: "84"
    name: "Dublin 11, Glasnevin"
    lat: 53.372841
    lon: -6.286299
  - id: "76"
    name: "Dublin Co. - Lucan"
    lat: 53.362598
    lon: -6.441564
  - id: "65"
    name: "Dublin Co. - Sallynoggin"
    lat: 53.27775
    lon: -6.138075
  - id: "75"
    name: "Dublin 18, Sandyford"
    lat: 53.276107
    lon: -6.211814
  - id: "37"
    name: "Dublin 9, Santry"
    lat: 53.406802
    lon: -6.235792
  - id: "78"
    name: "Dublin Co. - Stillorgan (Plumb Centre)"
    lat: 53.279923
    lon: -6.218265
  - id: "77"
    name: "Dublin 8, Thomas St."
    lat: 53.342792
    lon: -6.277001
  - id: "83"
    name: "Dublin 12, Walkinstown (Plumb Centre)"
    lat: 53.315161
    lon: -6.335278
  - id: "26"
    name: "Carlow, Carlow Town"
    lat: 52.838706
    lon: -6.898736
  - id: "38"
    name: "Louth, Drogheda"
    lat: 53.718798
    lon: -6.346731
  - id: "23"
    name: "Wexford, Gorey"
    lat: 52.670619
    lon: -6.292118
  - id: "68"
    name: "Kilkenny, Kilkenny City"
    lat: 52.647232
    lon: -7.219475
  - id: "74"
    name: "Kildare, Naas"
    lat: 53.212256
    lon: -6.680946
  - id: "61"
    name: "Meath, Navan"
    lat: 53.64823
    lon: -6.686599
  - id: "29"
    name: "Offaly, Tullamore"
    lat: 53.286165
    lon: -7.523551
  - id: "73"
    name: "Wexford, Wexford Town"
    lat: 52.304474
    lon: -6.460465
  - id: "34"
    name: "Limerick, Ballysimon"
    lat: 52.653103
    lon: -8.597482
  - id: "69"
    name: "Tipperary, Clonmel"
    lat: 52.363342
    lon: -7.690902
  - id: "54"
    name: "Clare, Ennis"
    lat: 52.837778
    lon: -8.973622
  - id: "70"
    name: "Cork, Mallow"
    lat: 52.139997
    lon: -8.648635
  - id: "72"
    name: "Cork, Midleton"
    lat: 51.90974
    lon: -8.174856
  - id: "71"
    name: "Tipperary, Nenagh"
    lat: 52.864457
    lon: -8.193372
  - id: "39"
    name: "Galway, Ballinasloe"
    lat: 53.331617
    lon: -8.228174
  - id: "82"
    name: "Galway, Galway City"
    lat: 53.283432
    lon: -9.030168
  - id: "47"
    name: "Roscommon, Roscommon Town"
    lat: 53.634874
    lon: -8.179253
  - id: "36"
    name: "Sligo, Sligo Town"
    lat: 54.275058
    lon: -8.48371
  - id: "20"
    name: "Donegal, Letterkenny"
    lat: 54.94991
    lon: -7.719598
  - id: "21"
    name: "Monaghan, Milltown"
    lat: 54.257139
    lon: -6.982664
  - id: "22"
    name: "Kerry, Tralee"
    lat: 52.274323
    lon: -9.674456
  - id: "25"
    name: "Mayo, Castlebar"
    lat: 53.856395
    lon: -9.283352
  - id: "24"
    name: "Waterford, Waterford City"
    lat: 52.243112
    lon: -7.155329
  - id: "35"
    name: "Louth, Dundalk (Daly's)"
    lat: 53.999834
    lon: -6.384292
  - id: "P1"
    name: "Panelling Centre, Dublin 9, Santry"
    lat: 53.405797
    lon: -6.239694
  - id: "28"
    name: "Meath, Kells (Formerly Rooneys)"
    lat: 53.728592
    lon: -6.887762
  - id: "14"
    name: "Heiton Steel, Dublin 22, Cut & Bent"
    lat: 53.31429
    lon: -6.37585
  - id: "11"
    name: "Heiton Steel, Dublin 22, Structural Steel"
    lat: 53.31429
    lon: -6.37585
  - id: "33"
    name: "Heiton Steel, Cork, Bishopstown"
    lat: 51.87323
    lon: -8.52557
concurrency: 2
---

# Chadwicks (chadwicks.ie)

Irish builders merchant (Chadwicks Ltd, Dublin Co. and nationwide branches), with a Heiton Steel division and a Plumb Centre and Panelling Centre sub-brand on some branches. Magento 2 storefront with Alpine.js and Klevu hosted search. Currency EUR. Prices are shown with VAT, and the default depends on whether you are logged in (see Prices). Note for readers: Heiton Buckley and Brooks are effectively the same group, so treat "Brooks" and "Heiton" stores and terms as one supplier family.

## Transport and access

- Search: `browser`. The Magento search URL (`/catalogsearch/result/?q=`) returns a shell without product rows over plain HTTP (no `.product-item` in the HTML). The rows come from Klevu, which the page calls by POST to `eucs31v2.ksearchnet.com`. The search page doesn't render Klevu rows in automated tabs (seen even without the guard), so `page` returns no rows. Search with the Klevu API instead: `tab_open` any chadwicks.ie page, then `evaluate` the snippet below. The guard allows this POST because the frontmatter lists it in `readPosts`.
- Product: `http`. The product page is server-rendered, about 1.8 MB, with a `Product` JSON-LD block (price inc VAT, `InStock`, 30-day returns).
- Policy: `http`. Delivery, click-and-collect and T&Cs pages are server-rendered. `/deliveryfaq` and `/clickandcollectfaq` return 301 to the slug-less form, so follow redirects.
- `/.well-known/ucp`: 404. No UCP.
- Robots and agent policy: not read in recon. Check `robots.txt` before raising concurrency.

## URL patterns

- Search (HTML shell): `https://www.chadwicks.ie/catalogsearch/result/?q={query}`. Results come from Klevu after load.
- Klevu search API (JSON; call it from a chadwicks.ie tab with the snippet below): `POST https://eucs31v2.ksearchnet.com/cs/v2/search`, public key `klevu-166800605309315648` (search and analytics, from the page's `klevu_jsapikeys`). Body:
  ```json
  {"context":{"apiKeys":["klevu-166800605309315648"]},"recordQueries":[{"id":"productlist","typeOfRequest":"SEARCH","settings":{"query":{"term":"drill"},"typeOfSearch":"AND","searchPrefernce":"","sort":"RELEVANCE","analytics":{"ipAddress":"0.0.0.0"}},"filters":{"filtersToApply":[]},"limit":12,"offset":0}]}
  ```
  Note `filters` must be an object. An array returns 500 on this host. Host `eucs33v2` also returns 500. `eucs31v2` returns 200 with `totalResultsFound` (179 for "drill"). Pages: `offset`.
- Product: `https://www.chadwicks.ie/<slug>-<sku>.html`, e.g. `dewalt-dck266p2-brushless-combi-impact-kit-53094.html`. Magento product with SKU at the end of the slug in most cases. Klevu `url` values are the canonical form.
- Stores: `https://www.chadwicks.ie/stores/` (list, 42 branches, Load Next pagination). Branch page: `https://www.chadwicks.ie/stores/<slug>`.
- Category tree: hire lives under `/all-products/hire-1/...`. Hire items are not retail and carry hire prices (e.g. the Makita "Cordless SDS Drill C/W Combi Drill" at EUR 32.64 is a hire price). Filter them out of retail rankings by breadcrumb (`Hire`).

## Extraction hints

Product page:
- JSON-LD `@type: Product`: `name`, `sku` (Chadwicks SKU, e.g. `53094`), `offers.price` (string, inc VAT for anonymous), `offers.priceCurrency` (`EUR`), `offers.availability` (`https://schema.org/InStock`), `offers.priceValidUntil`, `offers.hasMerchantReturnPolicy` (`merchantReturnDays: 30`, `returnFees: FreeReturn`, applicable country IE). `brand` and `gtin13` were null.
- Breadcrumb JSON-LD (`BreadcrumbList`): category path, including hire.
- Ex-VAT price is in the DOM, not JSON-LD: the rendered `customerPrice_excl_vat` text (`€279.12 ex VAT` on the DeWalt kit).

Klevu JSON (when reachable), `queryResults[0].records[]`:
- `name` (title), `sku` (often the title, not a SKU, so use `url`), `url` (canonical product URL).
- `priceInclTax`, `priceExclTax`, `price`. For retail items `price` equals `priceExclTax`, so don't display `price` as the anonymous price. For this doc use `priceInclTax` for the anonymous view.
- `inStock` (`yes`/`no`), `delivery_type` (`courier` on large items), `deliveryInfo` (empty in sample), `storeBaseCurrency` (`EUR`), `imageUrl`, `category`, `klevu_category`, `totalVariants`, `startPrice`/`toPrice` (ranges).
- `meta.totalResultsFound` for counts.

## Prices and VAT

- Anonymous default: inc VAT. Alpine store `vatStatus.incVATLoggedOut` is persisted `true`, so the default is inc VAT. A browser check on the DeWalt kit showed `€343.32 inc VAT` as the main price and `€279.12 ex VAT` as the secondary line. The `€6.50 inc VAT` delivery line is also inc VAT.
- Toggle: the header and PDP VAT toggle calls `toggleVAT()`. For non-B2B customers it flips `incVATLoggedOut`. For B2B customers it flips `incVAT`.
- Trade login (not tested, per brief): the code shows a separate `incVAT` store that is persisted `false` by default. A logged-in B2B account therefore sees ex-VAT by default. Klevu also takes `klevu_loginCustomerGroup` (customer group), so trade pricing may differ by group. Trade pricing is "special pricing for trade customers only" per the site header. Don't log in to verify.
- Ex VAT and inc VAT are both shown on product pages. Use the inc VAT figure for comparisons with other UK/IE retailers, and say which one you used.
- VAT rate: 23% (`priceInclTax / priceExclTax` = 1.23 on the samples).

## Stock

- Online stock is a yes/no flag (`inStock`, JSON-LD `availability`). No quantity.
- Click and collect: per branch, all 42 at once. `POST /graphql` (header `Store: default`) `stockDetails(sku)` returns `branchCode`, `branchSellableQty` and `branchStatus` (1 available, 2 out, 3 available with extended lead times) for every branch. `branchCode` is the `stores` id here (not the id on the `/stores/` pages). The adapter's `pickup` uses it; no branch selection needed.
- Reviews: none. The site has no review service (rating counts are always 0).
- Hire items: `hire` breadcrumb, not stock.

## Stores near Dublin

Source: `https://www.chadwicks.ie/stores/<slug>` store pages (`latitude` and `longitude` JSON on each page, address in the page `title`). Hours and phone not captured.

| Slug | Store | Address | Lat, lon |
|---|---|---|---|
| glasnevin | Dublin 11, Glasnevin | Glasnevin Industrial Estate, Glasnevin, D11 W729 | 53.372841, -6.286299 |
| robinhood-road | Dublin 12, Robinhood Rd | Robinhood Road, off Long Mile Road, Clondalkin, D22 V5T3 | 53.321304, -6.349288 |
| walkinstown | Dublin 12, Walkinstown (Plumb Centre) | Greenhills Road, Walkinstown, D12 V298 | 53.315161, -6.335278 |
| thomas-street | Dublin 8, Thomas St. | 66-68 Thomas Street, D08 X7T2 | 53.342792, -6.277001 |
| east-wall-road | Dublin 3, East Wall Road | Merchants Yard, East Wall Road, D03 H3V1 | 53.353134, -6.226143 |
| santry | Dublin 9, Santry | Unit 1, Constellation Road, Airways Industrial Estate, D17 YN12 | 53.406802, -6.235792 |
| the-panelling-centre-santry | Panelling Centre, Dublin 9, Santry | Furry Park Industrial Estate, Swords Road, D09 FW60 | 53.405797, -6.239694 |
| coolock | Dublin 17, Coolock | Greencastle Parade, Coolock, D17 F963 | 53.394446, -6.194156 |
| donabate | Dublin Co., Donabate | Turvey Business Park, K36 WD90 | 53.492806, -6.193371 |
| lucan | Dublin Co., Lucan | Laraghcon, Lucan, K78 K4D8 | 53.362598, -6.441564 |
| sandyford | Dublin 18, Sandyford | Ravens Rock Road, Sandyford Industrial Estate, D18 KR25 | 53.276107, -6.211814 |
| stillorgan | Dublin Co., Stillorgan (Plumb Centre) | Birch Avenue, Stillorgan Industrial Park, A94 A3X6 | 53.279923, -6.218265 |
| sallynoggin | Dublin Co., Sallynoggin | Sallynoggin Road, A96 X7R0 | 53.277750, -6.138075 |
| bray | Wicklow, Bray | Boghall Road, Bray, A98 HD32 | 53.187892, -6.104491 |
| naas | Kildare, Naas | Newbridge Road, Naas, W91 XN7T | 53.219653, -6.664047 |
| heiton-steel-cut-bent | Heiton Steel, Dublin 22, Cut & Bent | Nass Road, Clondalkin, D22 W8Y8 | 53.314290, -6.375850 |
| heiton-steel-structural-steel | Heiton Steel, Dublin 22, Structural Steel | Nass Road, Clondalkin, D22 W8Y8 | 53.314290, -6.375850 |
| navan | Meath, Navan | Carriage Road, Navan, C15 NW64 | 53.648230, -6.686599 |
| rooneys | Meath, Kells (formerly Rooneys) | Old Castle Road, Kells, A82 P9H0 | 53.728592, -6.887762 |
| drogheda | Louth, Drogheda | Scarlet Street, Drogheda, A92 T268 | 53.718798, -6.346731 |

The two Heiton Steel rows share a site. Near Maynooth (53.381, -6.591): Lucan, Naas, Glasnevin, Robinhood Road and Walkinstown are the closest on the list. Stores with no page fetched: Dublin Co. Balbriggan-area and Swords are not in the list.

## Delivery and collection

Source: `https://www.chadwicks.ie/deliveryfaq` (inc VAT, from the page):
- Small items: EUR 6.50 inc VAT, next working day if ordered before 1pm (not if the order has bulky items). In-stock items only for the next-day promise.
- Large items: EUR 29.99 inc VAT, 1 to 2 working days.
- Delivery zones and the "contact you" flow for unsuitable areas are on the same page. Zone list not captured.

Source: `https://www.chadwicks.ie/clickandcollectfaq`:
- Click and collect is free. Ready in about 1 hour for in-stock items, per the header banner. Collect with the order confirmation email.
- Held for collection: the branch contacts you within 24 hours if you haven't collected. Uncollected orders are cancelled and refunded.

## Returns and warranty

- Product JSON-LD: 30-day merchant return window, returns by mail, free return (IE). Use this as the source for returns.
- Terms (`/terms-and-conditions`): a sale-of-goods T&C with a 14-day window for non-delivery claims and 7 days for shortages. The T&Cs don't set the general returns rule. Returns in store: contact the branch where you bought (click-and-collect FAQ).

## Price match

Not found in recon. Unknown.

## Quirks and risks

- Klevu POST is allowed via `readPosts` (decided 2026-10-14).
- Hire and retail share one catalog. Hire prices show in search results. Filter by breadcrumb.
- Trade account pricing is gated. Recorded from code only.

## Search snippet (Klevu, read-only)

Run with `evaluate` in a tab opened on any chadwicks.ie page; replace the query.

```js
async () => {
  const body = {
    context: { apiKeys: ["klevu-166800605309315648"] },
    recordQueries: [{ id: "productlist", typeOfRequest: "SEARCH", settings: { query: { term: "cordless drill" }, typeOfSearch: "AND", sort: "RELEVANCE" }, filters: { filtersToApply: [] }, limit: 24, offset: 0 }],
  };
  const r = await fetch("https://eucs31v2.ksearchnet.com/cs/v2/search", { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify(body) });
  const j = await r.json();
  // Klevu `price`/`salePrice` are ex VAT on retail items: multiply by 1.23 or read the inc-VAT price on the product page.
  return (j.queryResults?.[0]?.records ?? []).map((x) => ({ title: x.name, exVat: x.salePrice ?? x.price, inStock: x.inStock, url: x.url, sku: x.sku }));
}
```

