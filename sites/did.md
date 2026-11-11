---
id: did
name: DID Electrical
domains: [did.ie]
kind: retailer
categories: [electronics]
urls:
  search: "https://www.did.ie/search/suggest.json?q={query}&resources[type]=product&resources[limit]=10"
  search-html: "https://www.did.ie/search?q={query}&type=product"
  check-search: "https://www.did.ie/search?q=router&type=product"
  check-product: "https://www.did.ie/products/tp-link-be3600-dual-band-wi-fi-7-router-black-archer-be230.json"
defaults: { read: on, write: ask }
blocked: ["/cart", "/checkout", "/account", "/orders", "/admin", "*/cart*", "*/checkout*", "*/account*", "*/orders*"]
ucp: { endpoint: "https://www.did.ie/api/ucp/mcp" }
concurrency: 2
---
DID Electrical, Irish electronics and appliance retailer (Dublin HQ, about 25 stores in the Republic of Ireland). Shopify storefront behind Cloudflare. Prices in EUR, shown with VAT included. Ships within Ireland. Not verified: whether it ships to Northern Ireland.

## Access rules (read first)

- `robots.txt` and `/agents.md` ask agents to identify agent-initiated requests, to back off on 429, and to use the store's UCP/WebMCP tools for any shopping action. `/cart.js` and `/recommendations/products` are disallowed for crawlers. Don't read them, and never add to cart or check out.
- `agents.md` says server-to-server requests should use Web Bot Auth. Plain HTTP from this plugin does not do that. Keep `concurrency` low, and treat this as an open question (see the recon file).
- Store UCP profile: `https://www.did.ie/.well-known/ucp`. Its MCP endpoint is `https://did-electrical.myshopify.com/api/ucp/mcp`. Not used by this site doc. Read-only discovery only.

## Transport

- search: `http`. Use `suggest.json` for data. `page` returns the raw JSON as text, and one search page (10 results with HTML `body`) is about 14k tokens, so `page` truncates at its default budget. Use `maxTokens` at 20000, or read the JSON directly and keep only the fields below.
- product: `http`. `/products/<handle>.json` (price string, variants, SKU, barcode). `/products/<handle>.js` (price in cents, `available`, variant `barcode`). The HTML product page is about 1.3 MB; `page` gives 17 sections, so use the JSON.
- stock: `http`, from `available`. Quantity is not exposed.
- policy: `http`. The policy and help pages are server-rendered.
- reviews: unknown. The product page has Okendo, Yotpo and Loox theme code, plus a `shopify-product-reviews` reference. Not tested.

## URL patterns

- Search (JSON): `https://www.did.ie/search/suggest.json?q={query}&resources[type]=product&resources[limit]=10`. Query is URL-encoded. Pages: not tested (`resources[limit]` max is 10 on suggest; use `/search?q=…&type=product&page=2` for more).
- Search (HTML): `https://www.did.ie/search?q={query}&type=product`. Reports the total ("137 results found for router").
- Product: `https://www.did.ie/products/<handle>` (HTML), `/products/<handle>.json`, `/products/<handle>.js`. Handles are in search results.
- Collections: `https://www.did.ie/collections/all/products.json?limit=5` returns JSON (250 max per page, not tested beyond 5). Collection handles: `/collections/<handle>`.
- Sitemap: `https://www.did.ie/sitemap.xml` (301 to the `www` host, then XML).
- Filters and sort: `robots.txt` blocks `sort_by` and `+` in collection URLs. Use the suggest or search `q` only.

## Extraction hints

Search (`suggest.json`), path `resources.results.products[]`:
- title: `title`. Handle: `handle`. URL: `url` (includes `_pos` and `_psq` tracking; strip them).
- price: `price` (string, `"106.99"`, EUR incl. VAT). `price_min`, `price_max` for variant range.
- was price: `compare_at_price_min` (string). `"0.00"` means none. Not verified on a discounted item.
- availability: `available` (bool).
- brand: `vendor` (e.g. `"TP Link"`, `"Starlink"`, `"DID Electrical"` for own warranty and house products).
- category: `type` (e.g. `"Routers & Hubs"`). Image: `image`.
- Model and SKU are not in suggest. Get them from the product JSON.

Product (`/products/<handle>.json`), path `product`:
- `vendor`, `product_type`, `tags` (includes `bc:attribute:…`, `Flow:in-store-only`, `Threshold:…`).
- Per variant: `price` (string, EUR incl. VAT), `compare_at_price` (null or `""` for none), `sku` (model code, e.g. `ARCHER BE230`), `barcode` (EAN-13, e.g. `4895252503685`), `available`, `inventory_management` (`shopify`), `inventory_quantity` (not returned: `null`).
- `body_html` holds the description and spec bullets.

Product (`/products/<handle>.js`): `price` in cents (`10699` = €106.99), `compare_at_price`, `available`, `vendor`. Per variant: `barcode`, `sku`, `weight` (grams).

Stock wording (HTML): `In Stock`, `Sold Out`, `Only N left` in the product template. Use the JSON `available` flag and say so.

Price format: `€106.99` in the UI, `"106.99"` in JSON. VAT is included. Ex-VAT is not shown.

## Delivery and collection (source: /pages/delivery)

- Standard small item: €5.99, plus €1.50 per extra small item. Maximum €29.99 for combined small items.
- Large item: €29.99. American fridge freezers €59.99 (two-man). Range cookers €49.99. Some products list €69.99 in the page's own table.
- Delivery 2–5 working days, in stock, Greater Dublin and DID Hub store areas. Some ROI areas use delivery partners with longer lead times. Mon–Fri, excluding bank holidays.
- Next-day delivery in Kilkenny, Killarney and Wexford (cut-off times apply).
- Same-day click and collect (`/pages/click-collect` is 404; use `/pages/delivery` and the store list).
- Installation: `Installation available within 3-5 business days` in selected counties.

## Stores

- Store list: `https://www.did.ie/pages/store-locations` (200). Store names, hours and addresses are not in the static HTML. About 25 stores (refund policy). Lat/lon: not found. Not verified.

## Returns, refunds and warranty (source: /policies/refund-policy, /pages/refunds-returns)

- Return within 30 days of purchase for a refund, credit note or exchange. Items must be in as-sold condition with original packaging. Ex-display items are not refundable. Promotional items must be returned too.
- Cancellation right: 30 days after receipt, under distance-selling rules. Returns must be sent within 30 days of cancelling. Standard delivery charge not refunded.
- Damage in transit: report to Customer Support within 3 days of delivery (01 625 0250, help@did.ie).
- Faults: bring to a store or contact Customer Support. The item goes to the manufacturer.
- Standard manufacturer warranty included (e.g. 1-year on installed appliances). Extended warranties sold as products, e.g. `1 years extended warranty` (`type: Warranty`, store-only flow).
- Domestic use only. Commercial use may void the warranty.

## Price match (source: /pages/price-match-promise)

- Price match against stock in another Republic of Ireland retailer: Currys, Power City, Harvey Norman, Expert, Euronics. Must be an exact match and in stock. Applies to items only, not delivery or installation. Price match on the day of purchase, online and in store. Ex-display items excluded. Also a "Double the Difference Guarantee".

## Reviews

- Not verified. The product page loads Okendo, Yotpo and Loox theme code (`okendoProduct.reviewAverageValue`, `okendoProductReviewCount`) plus a Shopify app reference. Which one is live is unknown. Critical (1–3 star) reviews: unknown. Open question.

## Quirks

- Cloudflare in front. Plain HTTP worked for every endpoint in this recon, with no challenge page.
- Product HTML is 1.3 MB and mostly theme and app scripts. Prefer the JSON endpoints.
- Search results include warranty and accessory products (e.g. `extwaranty134…`). Filter by `type` or `vendor`.
- Some `.js` fields are `null` (top-level `sku`, `barcode`). Read them from `variants[0]`.
- `/products/<handle>.json` price is a string. Parse it as a decimal, not an integer.

## Sources (checked 2026-10-14)

- `https://www.did.ie/robots.txt`, `https://www.did.ie/agents.md`, `https://www.did.ie/.well-known/ucp`
- `https://www.did.ie/pages/delivery`, `https://www.did.ie/pages/price-match-promise`, `https://www.did.ie/policies/refund-policy`, `https://www.did.ie/pages/refunds-returns`, `https://www.did.ie/pages/store-locations`

## Catalog via UCP (preferred for search and product data)

did.ie serves a UCP endpoint (the store's sanctioned agent API, per its `agents.md`). Use the `catalog` tool instead of scraping:
- `catalog {site: "did", query: "wifi 7 router", limit: 10}`: products with title, URL, vendor, price (EUR, inc VAT), availability, variants
- `catalog {site: "did", ids: ["gid://shopify/Product/…"]}` or `{site: "did", id: "gid://…"}` for details
- Filters: `minPrice`/`maxPrice` in euros, `available`
- Results aren't cached (Shopify's terms). Reviews and delivery aren't in UCP: use the HTML/JSON pages above

