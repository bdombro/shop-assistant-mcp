---
id: strahan
name: Strahan Timber
domains: [strahanonlineshop.ie]
kind: retailer
categories: [home-improvement]
urls:
  search: "https://www.strahanonlineshop.ie/search/suggest.json?q={query}&resources[type]=product&resources[limit]=10"
  search-html: "https://www.strahanonlineshop.ie/search?q={query}&type=product"
  check-search: "https://www.strahanonlineshop.ie/search/suggest.json?q=timber&resources[type]=product&resources[limit]=10"
  check-product: "https://www.strahanonlineshop.ie/products/rwd-175x44-4-8m.json"
  shipping: "https://www.strahanonlineshop.ie/policies/shipping-policy"
  refunds: "https://www.strahanonlineshop.ie/policies/refund-policy"
defaults: { read: on, write: ask }
blocked: ["/cart", "/checkout", "/checkouts", "/account", "/orders", "/admin", "*/cart*", "*/checkout*", "*/account*", "*/orders*"]
ucp: { endpoint: "https://strahan-distribution-ltd.myshopify.com/api/ucp/mcp" }
concurrency: 2
---
Strahan Timber, Dublin timber merchant (softwood, hardwood, planed all over, sheet goods, mouldings, woodturning blanks, fixings and finishes). Shopify storefront. Prices in EUR, shown with VAT included. Ships within the Republic of Ireland only: not to Northern Ireland, England, Scotland, Wales or mainland Europe (shipping policy). Pickup at the Rathcoole yard is free.

## Access rules

- `robots.txt` and `/agents.md` ask agents to identify themselves, use the UCP/MCP endpoint for shopping, and get buyer authorisation for purchases. Read-only catalog lookups are fine. Don't add to basket or check out.
- `/cart` and `/checkout` are disallowed for crawlers. Never read them.
- Plain HTML search (`/search?q=`) returned HTTP 429 on the first call during recon. Use `search/suggest.json` for search, and keep `concurrency` at 2.

## Transport

- search: `http`, `search/suggest.json` (JSON, 10 products). Pages are not supported beyond `resources[limit]`. Use the `catalog` tool (UCP) for richer search.
- product: `http`, `/products/<handle>.json`. The HTML product page is about 460 kB.
- policy: `http`, `/policies/shipping-policy` and `/policies/refund-policy` (server-rendered, text in `<main>`).
- stock: `http` from variant `available` on `products.json`. No store-level stock.

## Catalog via UCP (preferred for search and product)

- `catalog {site: "strahan", query: "…", limit: 10}`. Shopify storefront MCP, same catalog as the site.
- Results aren't cached (Shopify terms).

## URL patterns

- Search (JSON): `https://www.strahanonlineshop.ie/search/suggest.json?q={query}&resources[type]=product&resources[limit]=10`
- Product: `https://www.strahanonlineshop.ie/products/<handle>` (HTML), `.json`, `.js`. Handles are in search results.
- Full catalogue: `https://www.strahanonlineshop.ie/products.json?limit=250&page=N` (250 products per page; about 250 on page 1 at recon).
- Collections: `https://www.strahanonlineshop.ie/collections/<handle>`, e.g. `building-material`, `planed-hardwoods-softwoods`, `birch-plywood-2440mm-x-1220mm-bb-bb`, `hardwood-dowels`, `plain-mdf`.

## Extraction hints

Search (`suggest.json`), path `resources.results.products[]`:
- title: `title`. Handle: `handle`. URL: `url`.
- price: `price` (string, EUR incl. VAT). `price_min`, `price_max` for variant range.
- availability: `available` (bool, any variant in stock).
- category: `type` (often empty). Vendor: `vendor` is "Strahan Timber." for own products.

Product (`/products/<handle>.json`), path `product`:
- `title`, `handle`, `body_html`, `vendor`, `product_type`, `tags`, `options`.
- Per variant: `title`, `option1` (size or length), `price` (string), `compare_at_price` (null = none), `sku`, `grams` (weight), `available` (may be absent on some products, e.g. the beech PAO lines; check `products.json`).

Tags to read: `Pickup Only`, `Shipping Only`, `No Shipping`, `Click and Collect Only` (see Delivery). Also `Rough`, `Planed`, `PAO` for finish, and `White Deal`, `Hardwood` for species.

## Timber: dimensions, lengths and price

Product titles carry the nominal section and length, in this order: `<width>mm x <thickness>mm <species/finish> <grade> <length>`. Examples:
- `175mm x 44mm White Deal Rgh C16  4.8Mt.` (`rwd-175x44-4-8m`): 175x44 rough sawn C16 white deal, 4.8 m. €22.12 per piece, 30 kg (`grams: 30000`).
- `50 x 22 White Deal Rgh  2.4M`, `175 x 22 White Deal Rgh 4.8M`, `RED CEDAR PAO 144MM X 19MM X 3.66M`.
- `Steamed Beech Planed All Over - 2400MM (2.4mt)`: one product, eight section variants in `option1` (e.g. `44MM x 20MM` €12.55 per 2.4 m, `194MM x 44MM` €67.10 per 2.4 m).
- `Steamed Beech Planed All Over`: one product, 600 mm lengths, variants `600MM X 44MM X 20MM`, etc.

Format:
- Price is **per piece (per length)**, not per metre. Work out €/m as `price / length_m`. Lengths are in the title (`4.8M`, `5.4M`, `2.4mt`).
- Length variants are usually separate products (one handle per length, e.g. `rwd-175x44-4-8m`, `rwd-175x44-4-2m`, `rwd-175x44-3-6m`). Compare them in one search, not as variants.
- Section variants (e.g. planed 2.4 m beech) are in `variants[].option1`.
- Veneers: `SQ/MT` (price per square metre). Plywood and MDF: per sheet.
- Nominal sizes (e.g. `175 x 44`) are the sawn size. Planed (PAO) sizes are smaller. Check the title, not the species.
- Weight: `grams` per piece (e.g. 30000 for 4.8 m 175x44 rough). Use it for delivery and lifting checks.

## Delivery (source: `/policies/shipping-policy`)

- Delivery is in Ireland only. Not available to Europe, England, Scotland, Wales or Northern Ireland.
- Standard delivery: "Otherwise, we'll deliver within a reasonable time." No fixed days stated.
- Fees: "Confirmed at checkout." No table on the page.
- Specific day: "failure to do so will incur an extra delivery charge (*Local Deliveries only)".
- Long items: no delivery policy stated for lengths. Product tags say `Shipping Only` (parcel) or `No Shipping` (collection only). 61 of 250 products carry `No Shipping`, including many long timbers (4.8 m, 5.4 m). For those, yard pickup looks like the only option. Open question: whether delivery is quoted for them.
- Click & collect: the store says "Please do not travel to collect your order until you receive an SMS confirming that it is ready for collection."
- Free shipping at checkout applies to pickup and collection orders only.
- Some liquids and paints can't go by parcel.
- Notify changes or cancellations as early as possible. Inspect on arrival and report damage or missing items within 48 hours.

## Pickup (yard)

- Showroom, Block 3, Unit B4–B5, Aerodrome Business Park, Rathcoole, Co. Dublin, D24 X371. Source: shipping-policy and contact pages.
- Opening hours (source: site footer): Mon–Thu 07:45–17:00 (lunch 13:00–13:30); Fri 07:45–16:00 (lunch 13:00–13:30); Sat, Sun, bank holidays and Good Friday closed.
- Coordinates: not published. Approx. 53.27, -6.42 (from the address, not verified). Use only after checking.
- Phone: (01) 401 9100. Email: sales@strahan.ie (from the site header).

## Returns and refunds (source: `/policies/refund-policy`)

- Cooling-off: cancel for any reason within 14 days of delivery. Perishables are excluded.
- Return in unused, saleable condition, in original packaging, with an RMA number from the Sales Team. The buyer pays return postage unless the item is faulty or was sent in error. If not returned within 14 days of cancelling, Strahan may collect and charge the collection cost.
- Refunds: 5–10 business days (Shopify), depending on the bank.
- Warranty: not in the policy text checked.

## Price match and offers

- Not found on the policy pages checked.

## Quirks

- Most products have an empty `product_type`. Use `tags` for category.
- Some products show `available: false` in JSON even when a title looks in stock. Read the flag, don't guess from the title.
- Some variant fields are absent (not `null`) on certain products (e.g. `available`). Use `.get`.
- Titles are inconsistent: `4.8M`, `4.8Mt.`, `2.4mt`, `3.66M`. Parse with a regex on `(\d+(\.\d+)?)\s*(m|mt|mts|mtr)\b`, case-insensitive.
- Some sizes are in the handle only (e.g. `rwd-175x44-4-8m`).

## Sources (checked 2026-10-14)

- `https://www.strahanonlineshop.ie/robots.txt`, `/agents.md`, `/.well-known/ucp`
- `https://www.strahanonlineshop.ie/policies/shipping-policy`, `/policies/refund-policy`
- `https://www.strahanonlineshop.ie/` (footer: showroom address and hours)
- `https://www.strahanonlineshop.ie/search/suggest.json?q=timber`, `/products.json?limit=250`, `/products/rwd-175x44-4-8m.json`, `/products/steamed-beech-planed-all-over-2400mm.json`
