---
id: woodies
name: Woodie's (Ireland)
domains: [woodies.ie]
kind: retailer
categories: [home-improvement]
urls:
  search: https://www.woodies.ie/catalogsearch/result?q={query}
  graphql: https://www.woodies.ie/graphql
  check-search: https://www.woodies.ie/catalogsearch/result?q=einhell+drill
  check-product: https://www.woodies.ie/einhell-18v-pro-combi-drill-bare-unit
  stores: https://www.woodies.ie/storelocator
  click-and-collect: https://www.woodies.ie/click-collect
  delivery: https://www.woodies.ie/delivery-information
  returns: https://www.woodies.ie/return-policy
defaults:
  read: on
  write: ask
readPosts:
  - /graphql
blocked:
  - /checkout
  - /customer
  - /sales/order
  - /wishlist
  - /checkout/cart/add
  - "*/checkout/*"
  - "*/customer/*"
allowedPages: []
concurrency: 2
---

# Woodie's (woodies.ie)

Magento 2 storefront (PWA Studio build: `/dist/`, Apollo cache in the page, Algolia-powered search suggestions). Irish DIY and home retailer, 36 stores. Currency EUR. Prices look like they include VAT (Irish retail convention), not yet verified.

## URL patterns

- Search: `https://www.woodies.ie/catalogsearch/result?q={query}`. The HTML page renders results client-side. The `page` tool on this URL returns only the "Customers who searched" Nosto recommendation block (no result rows, "products found — Page NaN"). Use the GraphQL endpoint below for rows. Redirects: `/catalogsearch/result/?q=` 301s to the no-slash form.
- Product: `https://www.woodies.ie/<url_key>` (no `.html` suffix; the `.html` form is a 404). Brand pages are `/brand/<brand>` (e.g. `/brand/einhell`).
- Stores: `/storelocator` (36 stores, Apollo cache `ROOT_QUERY.stores.N`). Store page `/storelocator/<slug>`.
- Click and collect: `/click-collect`. In-store services: `/in-store-services`.
- Help: `/delivery-information`, `/return-policy`.

## GraphQL (the search data path)

Magento GraphQL is public and answers read queries over plain HTTP with a browser UA and `Store: default` header. Introspection is disabled. Search rows:

```
POST https://www.woodies.ie/graphql
Content-Type: application/json
Store: default

{"query":"{ products(search: \"einhell drill\", pageSize: 20) { total_count page_info { total_pages } items { name sku url_key stock_status store_availability { status } price_range { minimum_price { final_price { value currency } regular_price { value } } } } } }"}
```

- Paging: `currentPage: n` on `products(...)`. `total_count` for "einhell drill" was 626.
- Row fields: `name`, `sku`, `url_key` (product URL = `/<url_key>`), `stock_status` (`IN_STOCK`), `store_availability.status` (a string such as `Available in all stores` or `Not available in stores`), `price_range.minimum_price.final_price.value` (EUR number).
- `store_availability.status` is not per store (`Available in all stores`, `Selected Stores`, `Not available in stores`). Per-store quantities come from the `productStock` query (see Branch stock).

## Branch stock and reviews

Adapter: `sites/woodies.ts` supports search, offer, pickup (`near`) and reviews.

- Branch stock: GET `/graphql?query=…&variables=…` with `query nearbyProductStock($productIds:[Int!]!,$latitude:Float!,$longitude:Float!){productStock(product_ids:$productIds,latitude:$latitude,longitude:$longitude){distance store{name store_code latitude longitude} stock{products{id sku stock{qty status}}}}}`. `productIds` are Magento ids (`id` on a product, not the SKU). It lists every store, nearest first, with `qty` and `status` (`in`/`out`), `distance` in km. The product page's "Choose store" dialog (address search) calls it; no store needs to be selected.
- Reviews: Yotpo storefront API, `GET https://api-cdn.yotpo.com/v3/storefront/store/<key>/product/<magento id>/reviews?page=1&perPage=N&sort=rating&direction=asc` (lowest first; `sort=date&direction=desc` newest). The `<key>` is in the page's Yotpo loader script URL (`cdn-widgetsrepository.yotpo.com/v1/loader/<key>`).
- Only read queries. Don't send mutations (`addProductsToCart`, `createCustomer`, etc.).

Copyable `evaluate` recipe (read-only; run it on any woodies.ie page so the request is same-origin):

```js
async () => {
  const q = encodeURIComponent("einhell drill");
  const r = await fetch("/graphql", { method: "POST", credentials: "omit",
    headers: { "Content-Type": "application/json", Store: "default" },
    body: JSON.stringify({ query: `{ products(search: "${q}", pageSize: 20) { total_count items { name sku url_key stock_status store_availability { status } price_range { minimum_price { final_price { value } regular_price { value } } } } } }` }) });
  const { data } = await r.json();
  return { total: data.products.total_count, rows: data.products.items.map((p) => ({
    title: p.name, price: p.price_range.minimum_price.final_price.value,
    wasPrice: p.price_range.minimum_price.regular_price.value,
    stock: p.stock_status, storeAvailability: p.store_availability?.status,
    url: "https://www.woodies.ie/" + p.url_key })) };
}
```

Note: `wasPrice` equals `regular_price`, and it only differs from `final_price` when a discount applies. Check against the page before quoting it.

## Extraction hints

- Product HTML (plain HTTP works): the SSR markup has `<span class="sku">`, `<span class="price">89.99</span>`, `<span class="price_currency_code">EUR</span>` and `<span class="availability">InStock</span>`. Those are server-rendered (see the `sku` and `price` spans in the product fixture). Breadcrumbs: `/DIY & Building Materials`, brand as `brand/<brand>`.
- Product JSON-LD: only Organization and WebSite. No `Product` schema.
- Store list: parse the Apollo state `ROOT_QUERY.stores.N` blocks: `name`, `city`, `url`, `address`, `latitude`, `longitude`, `opening_hours`, `bank_holiday_hours`.

## Stores near Dublin (lat/lon from the store locator)

- Glasnevin (Dublin Industrial Estate): 53.372378, -6.284963, `/storelocator/glasnevin`
- Blanchardstown: 53.39553, -6.392498, `/storelocator/blanchardstown`
- Coolock (Malahide Road): 53.394692, -6.190479, `/storelocator/coolock`
- Airside (Swords): 53.446864, -6.224921, `/storelocator/airside`
- Swords Seatown: 53.461367, -6.211785, `/storelocator/swords-seatown`
- Lucan (Ballydowd): 53.358722, -6.423869, `/storelocator/lucan`
- Naas Road (Dublin 12): 53.323374, -6.354832, `/storelocator/naas-road`
- Tallaght: 53.283368, -6.377247, `/storelocator/tallaght`
- Sandyford: 53.27444, -6.206095, `/storelocator/sandyford`
- Carrickmines: 53.250631, -6.186354, `/storelocator/carrickmines`
- Sallynoggin: 53.277981, -6.139126, `/storelocator/sallynoggin`
- Bray: 53.17964, -6.130747, `/storelocator/bray`
- Naas (Monread Road): 53.23458, -6.638253, `/storelocator/naas`
- Newbridge: 53.173994, -6.804963, `/storelocator/newbridge`
- Navan: 53.655824, -6.701542, `/storelocator/navan`
- Drogheda: 53.727344, -6.390384, `/storelocator/drogheda`
- Mullingar: 53.531977, -7.310886, `/storelocator/mullingar`
- Dundalk: 53.99238, -6.37791, `/storelocator/dundalk`

Nearest to Maynooth (53.381, -6.591) are Lucan, Blanchardstown and Naas. The full list of 36 stores is in the locator page.

## Policy (source URLs)

- Delivery (`/delivery-information`): Next Day Delivery €7.95 (Mon-Fri orders before 3pm, next working day). Standard €5.95, 2-3 working days. Large €15, 2-3 working days. Bulky €55, up to 5 working days. 4 Hour Click & Collect €0, ready within 4 working hours. Some items come from suppliers and take up to 10 working days. Charges are weight and dimension based and shown at checkout.
- Click and collect (`/click-collect`): free, "ready within 4 business hours". Orders placed within 4 hours of closing are ready next day. Collect at the Customer Service desk with the second "ready for collection" email and the order number.
- Returns (`/return-policy`): 30 days to any of the 36 stores for a refund or exchange, in re-saleable condition, with receipt and card. Faulty items within warranty: exchange, refund or repair. Paint returns in-store only. Balloons, mixed paint, plants, roll-out turf, tonne bags, fuel pallets and installed showers can't be returned. Drop2Shop returns at all 36 stores.
- Price match: not checked.

## Quirks and risks

- Cloudflare in front, no bot challenge seen on plain HTTP with a browser UA.
- Search HTML has no product rows; the search result page depends on JS. Use GraphQL.
- Magento GraphQL is a POST. `readPosts: [/graphql]` lists it as read-only. Verify that the policy layer allows it before relying on it.
- Most products have no reviews.
- Cookie consent (CookieFirst) overlay; decline or necessary only.

## Open questions

- VAT: prices are presumed incl. VAT. Not verified.
