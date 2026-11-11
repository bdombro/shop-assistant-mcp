---
id: harveynorman
name: Harvey Norman Ireland
domains: [harveynorman.ie]
kind: retailer
categories: [electronics, home-improvement]
urls:
  search: "https://www.harveynorman.ie/search?q={query}&type=product"
  check-search: "https://www.harveynorman.ie/search?q=dishwasher&type=product"
  check-product: "https://www.harveynorman.ie/products/electrolux-600-smartselect-built-in-dishwasher-13-place-e62lb100s"
  stores: "https://www.harveynorman.ie/pages/store-finder"
  delivery: "https://www.harveynorman.ie/pages/delivery-information"
  click-and-collect: "https://www.harveynorman.ie/pages/click-and-collect"
  returns: "https://www.harveynorman.ie/pages/returns-policy"
  price-match: "https://www.harveynorman.ie/pages/best-price-guarantee"
defaults: { read: on, write: ask }
blocked: ["/cart", "/checkout", "/checkouts", "/account", "/orders", "*/cart*", "*/checkout*", "*/account*", "*/orders*"]
ucp: { endpoint: "https://messagepool-hnie-prod.myshopify.com/api/ucp/mcp" }
concurrency: 2
---
# Harvey Norman Ireland (harveynorman.ie)

Shopify storefront (shop `messagepool-hnie-prod`) behind Cloudflare. EUR, prices include VAT, whole euros on listing cards. 16 stores in the Republic of Ireland. Sells electrical and technology, appliances, furniture and bedding. Categories: electronics, home-improvement.

## Transport

- Plain HTTP (curl with a Chrome UA, and the `fetch`/`probe` CLI) gets a Cloudflare "Just a moment..." 403 on every path, including `/`, `/robots.txt` and `/.well-known/ucp`. So all page kinds are `browser`.
- Inside a browser tab, same-origin `fetch()` of Shopify endpoints works. Two parallel fetches triggered a challenge page once; space them a few seconds apart and check the title is not "Just a moment...".
- UCP: `https://www.harveynorman.ie/.well-known/ucp` (via browser) lists MCP endpoint `https://messagepool-hnie-prod.myshopify.com/api/ucp/mcp`. That host is not behind the storefront Cloudflare, so the `catalog` tool should work without a browser. Prefer `catalog` for search.

## URL patterns

- Search: `/search?q={query}&type=product`. 20 products per page, header says "Search: 531 results found for "dishwasher"". Page 2: `&page=2`.
- Suggest JSON: `/search/suggest.json?q={query}&resources[type]=product&resources[limit]=10` (returns `body`, `available`, price).
- Product: `/products/<handle>` (handle ends with the model number, lower-case). `.js` gives JSON: `variants[0].{id,sku,price,compare_at_price,available}`; price is in cents (54900 = EUR 549.00). Page HTML is about 2.6 MB raw (about 1.3 MB with scripts stripped); prefer `.js` or JSON-LD.
- Collections: `/collections/<handle>` (e.g. `kitchen-appliances`, `dishwashers`). Breadcrumb in JSON-LD `BreadcrumbList`.
- Reviews: Bazaarvoice (`harvey-norman-en_ie` deployment), client-rendered. Not tested.

## Extraction hints

- Search rows: custom element `<product-card>`. Link `a[href*="/products/"]`, current price `.price--highlight` (text like `549`, prefixed "Starting From EUR"), old price `.list-price` (text `EUR899`, empty when no reduction; label "WAS" or "RRP"). Stock is not on cards (`show_stock_plp` and `show_stock_srp` are false in `#stock-transparency-settings`).
- Product JSON-LD: `ProductGroup` with `productID` (model number), `brand`, variants/offers; also `BreadcrumbList`.
- Stock on product pages: the "stock transparency" Shopify app (`show_stock_pdp: true`, proxy `/apps/stock-transparency-hnie-prod/api`) shows availability near a location (default lat/lon 53.2949, -6.3575, km; low-stock threshold 1, out-of-stock threshold 2). Needs a location choice in the UI, which is a state change; the proxy path guessed (`/api/stock`) returned 404. Online `available` from `.js` is the only anonymous signal.

Read-only `evaluate` for search rows:

```js
() => [...document.querySelectorAll("product-card")].map((c) => {
  const a = c.querySelector('a[href*="/products/"]');
  const num = (s) => (s ? Number(s.replace(/[^\d.]/g, "")) || null : null);
  return {
    title: a?.getAttribute("title") || c.textContent.replace(/\s+/g, " ").trim(),
    price: num(c.querySelector(".price--highlight")?.textContent),
    wasPrice: num(c.querySelector(".list-price")?.textContent),
    stock: null,
    url: a ? new URL(a.getAttribute("href"), location.origin).href : null,
  };
})
```

Title text on cards is the product name last in the card text; `alt` on the image template is also the full title. Verify the snippet in a real tab.

## Stores near Dublin

Coordinates are the centre of each store's Google Maps embed on the store-finder data (`https://hnie.blob.core.windows.net/hnie-assets/store-finder/roi-stores-data-peak.js`), so treat them as approximate (about 1-2 km). User at Maynooth (53.381, -6.591).

| Store | Address | Phone | Lat, lon (approx) |
|---|---|---|---|
| Blanchardstown | Unit 421, Blanchardstown Retail Park, Dublin 15, D15 AW70 | 01 824 7400 | 53.3956, -6.4634 |
| Fonthill | Unit 1, Fonthill Retail Park, Dublin 22, D22 YD89 | 01 572 9800 | 53.3472, -6.4067 |
| Naas | Units G-K, Newhall Retail Park, Naas, W91 HFW5 | 045 907 700 | 53.2092, -6.7379 |
| Tallaght | Corner of Airton and Greenhills Rd, Dublin 24, D24 E205 | 01 468 4500 | 53.2943, -6.3578 |
| Rathfarnham | Units 7A-7C, Nutgrove Retail Park, Dublin 14, D14 DD88 | 01 491 6300 | 53.2920, -6.3360 |
| Carrickmines | Unit 230, The Park, Dublin 18, D18 FH48 | 01 299 6900 | 53.2529, -6.2202 |
| Swords | Units 5-7, Airside Retail Park, Crowscastle, K67 H364 | 01 890 9900 | 53.4466, -6.2958 |
| Drogheda | Units 8-11, Drogheda Retail Park, A92 CC97 | 041 987 8200 | 53.7007, -6.4452 |

Nearest to Maynooth: Blanchardstown, Fonthill, then Naas. Other stores: Castlebar, Galway, Kinsale Road (Cork), Limerick, Little Island (Cork), Sligo, Tralee, Waterford. Hours (all stores; Thu/Fri evenings vary): Mon-Wed 09:30-18:00, Thu 09:30-19:00 to 21:00, Sat 09:30-18:00, Sun 11:00-18:00. Source: store-finder data above.

## Delivery and collection

Source: https://www.harveynorman.ie/pages/delivery-information , https://www.harveynorman.ie/pages/click-and-collect
- Republic of Ireland only (not Northern Ireland). Small items EUR 6.99 (DPD, 3-5 working days). Large electrical EUR 39.95; range cookers, American fridge freezers, TVs over 85 inch EUR 69.95. Furniture and bedding from EUR 49.00 (3-5 working days, independent carrier). Clearance 5-7 working days. Made-to-measure or lead-time items: they contact you.
- Free removal of old electrical appliances; sofa removal EUR 79, mattress EUR 30 (arrange at purchase). Installation/assembly on request, paid at point of sale.
- Click & Collect: free, 7 days a week, from any of 16 stores, ready "in as little as 2 business hours" for in-stock items. Bring driver's licence or passport, the email and the payment card. Car-park pickup at selected stores.

## Returns and warranty

Source: https://www.harveynorman.ie/pages/returns-policy
- Unless faulty, return within 14 days, preferably to the original store, with receipt, unopened or unused, resaleable, all components. Online orders: tell them within 14 days of delivery, then 14 days to send back. Return cost covered only if faulty.
- Excluded: clearance/ex-display/sold-as-seen, gift cards, started installation/assembly, custom or bespoke, opened software. Refund to original payment method.
- Manufacturer warranty per product; not checked on pages.

## Price match

Source: https://www.harveynorman.ie/pages/best-price-guarantee ("Best Price Guarantee")
- "MATCH or BEAT any genuine competitor's advertised price on the date you purchase or within 7 days of purchase", including competitor sale prices. Competitor must have physical retail stores in the Republic of Ireland, the item in stock and an identical model. Named: DID Electrical, Currys PC World, Powercity, Expert, Euronics, Soundstore.
- Excluded: Apple products, bundles, free items, limited quantities, negotiated prices, clearance, consumer-finance purchases. Online price matches consider delivery and extra costs. Bring the URL or advert.

## Quirks

- Cloudflare challenge for non-browser clients; stop on a CAPTCHA.
- Cart is a Shopify form POST (`/cart/add`); never submit.
- "Starting From" labels on cards: price may be for the cheapest variant.
