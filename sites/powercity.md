---
id: powercity
name: Powercity
domains: [powercity.ie]
kind: retailer
categories: [electronics]
urls:
  search: https://www.powercity.ie/groups/groups/search?search={query}
  check-search: https://www.powercity.ie/groups/groups/search?search=vacuum
  check-product: https://www.powercity.ie/product/081641
  stores: https://www.powercity.ie/groups/site/locations
  delivery: https://www.powercity.ie/groups/site/delivery
  returns: https://www.powercity.ie/groups/site/returns
  price-match: https://www.powercity.ie/groups/site/nofussprice
  warranty: https://www.powercity.ie/groups/site/warranty
defaults:
  read: on
  write: ask
readPosts: []
blocked:
  - /cart/
  - /groups/cart/
  - /product/cart-add
  - /site/register-email-offers
  - /site/quotebuilder
  - /site/contact
  - /groups/site/contact
allowedPages: []
strip:
  - "#top-menu"
  - "#customtabs"
  - ".menucolumn"
  - "#breadcrumbs-sticky"
  - "#cartheader"
  - "#featureform"
  - ".stock-filter-bar"
  - ".main1columnsfooter"
  - "footer"
stores:
  - { id: tallaght, name: Tallaght, lat: 53.289, lon: -6.372 }
  - { id: sallynoggin, name: Sallynoggin, lat: 53.27, lon: -6.135 }
  - { id: blanchardstown, name: Blanchardstown, lat: 53.39, lon: -6.38 }
  - { id: finglas, name: Finglas, lat: 53.389, lon: -6.3 }
  - { id: coolock, name: Coolock, lat: 53.393, lon: -6.2 }
  - { id: fonthill, name: Fonthill, lat: 53.328, lon: -6.418 }
  - { id: bray, name: Bray, lat: 53.203, lon: -6.11 }
  - { id: drogheda, name: Drogheda, lat: 53.718, lon: -6.356 }
  - { id: naas, name: Naas, lat: 53.216, lon: -6.67 }
  - { id: carrickmines, name: Carrickmines, lat: 53.248, lon: -6.196 }
  - { id: swords, name: Swords, lat: 53.458, lon: -6.218 }
concurrency: 2
---

# Powercity (powercity.ie)

Irish electrical retailer, Bray HQ, 11 stores in and around Dublin plus Drogheda. Yii (PHP) server-rendered pages, WebAngel hosting. Currency EUR, prices include VAT. No JSON API and no hosted search: plain HTTP with a browser User-Agent returns full HTML, including per-store stock. No bot wall seen on plain HTTP. The site redirects `http://` to `https://www.`.

## URL patterns

- Search: `https://www.powercity.ie/groups/groups/search?search={query}` (form field `search`, GET; the `q=` param returns 400). Keyword queries often 302 to a category listing instead of a results page, e.g. "vacuum" goes to `/groups/groups/view?grp=40&class=50` and "laptop" to `/groups/groups/view?grp=10&class=30`. Treat the final URL as the results page.
- Category: `/groups/groups/view?grp=<group>&class=<class>`. Filters are query params: `featureList=BRAND-DYSON,...` (feature values, uppercase), `in_stock=1`, `order_price=SORT_DESC`. Brand pages: `/groups/site/<brand>` (e.g. `/groups/site/dyson`).
- Product: `/product/<code>`, code is the 6-digit Powercity product code (e.g. `081641`, shown as "Product Code" on the page). Also `NVC115`-style codes exist.
- Paging: `.pagination` block on listings; follow its links (not verified in this recon).
- Store list: `/groups/site/locations`. Store pages are not separate URLs.
- Help: `/groups/site/delivery`, `/groups/site/returns`, `/groups/site/nofussprice`, `/groups/site/warranty`, `/groups/site/installation`.
- Blocked for agents: cart (`/groups/cart/index`, `/cart/view-transaction`, `/product/cart-add`), contact and quote forms, newsletter signup.

## Pages

Adapter `sites/powercity.ts` supports search, offer and pickup (no review service on the site). For `page` reads: search and category listings are server-rendered `div.group.product` blocks (titles truncated with "..."; the product page `h1` has the full name); product pages carry `Product Code`, `Warranty`, `#pricetag`, a features list and per-store stock. No JSON-LD, no `__NEXT_DATA__`.

## Price format quirks

- `€499.95` (euro sign, dot decimal, no thousands separator shown in listings, check on the page).
- Prices are inc VAT (23% on most lines). No ex-VAT price is shown.
- Price-match and was-price: the listing shows only one price; no strike-through value was seen in the sampled pages. Don't invent a was-price.
- Clearance and bundle deals appear in promo blocks; not parsed.

## Stock

- Home delivery stock is a count (`Home Delivery : 77 Units`). Listings show `Stock N Available` (the same total) or `Stock status: Displays Only`.
- Branch stock: `/product/stock-levels?id=<code>` (with `X-Requested-With: XMLHttpRequest`) lists every store with units, zeros included, plus the click-and-collect cutoff ("Until 18:00 Today"). Store names there: `Naas Co Kildare`, `Bray Co. Wicklow`, `Drogheda Co Louth`, the rest plain. The adapter matches them by the `stores` id prefix.

## Stores near Dublin

Source: `https://www.powercity.ie/groups/site/locations` (addresses and opening hours). Opening hours: 09:30-18:00 (the page shows Wednesday hours; check the day). Coordinates are NOT on the site: the `lat/lon` below and in frontmatter `stores` are approximate, from town or area centres, not from a map, and are unverified. Treat them as ±1 km.

| Store | Address | Lat, lon (approx.) |
|---|---|---|
| Tallaght | Greenhills Road, Tallaght, D24 AH32 | 53.289, -6.372 |
| Sallynoggin | Upper Sallynoggin Road, Co. Dublin, A96 XY58 | 53.270, -6.135 |
| Blanchardstown | Coolmine Retail Park, Blanchardstown, D15 VY56 | 53.390, -6.380 |
| Finglas | Church Street, Finglas, D11 K254 | 53.389, -6.300 |
| Coolock | UCI Complex, Malahide Road, D17 V660 | 53.393, -6.200 |
| Fonthill | Fonthill Retail Park, Fonthill Road, Clondalkin, D22 F226 | 53.328, -6.418 |
| Bray Co. Wicklow | Pinewood Close, Boghall Road, A98 WE54 (also HQ) | 53.203, -6.110 |
| Drogheda Co. Louth | Unit I, M1 Drogheda Retail Park, A92 H104 | 53.718, -6.356 |
| Naas Co. Kildare | Newhall, Naas, W91 XR96 | 53.216, -6.670 |
| Carrickmines | The Park, Carrickmines, D18 EF9H | 53.248, -6.196 |
| Swords | Airside, Swords, K67 V3W5 | 53.458, -6.218 |

## Delivery and collection

Sources: `/groups/site/delivery`, `/groups/site/locations`.

- Click & Collect: items in stock at a store can be collected immediately. Store page says: "If the items you require show in stock, you can collect immediately." Cutoff shown as "until 18:00 Today".
- Local Dublin delivery: daily, all items, to listed Dublin areas, excluding Sundays and bank holidays. Charge is shown in the basket ("click the buy button to put the item in your shopping basket"); the charge was not in the static page text.
- Wicklow, Kildare, Meath, Louth: 1-2 day delivery, same exclusions.
- Large items nationwide: outside couriers, 12-15 working days.
- Missed delivery: extra charge.
- Installation and disconnect: separate service (`/groups/site/installation`).
- Pre-Christmas notices on the delivery page are dated 2024; ignore them.

## Returns and warranty

Source: `/groups/site/returns`, `/groups/site/warranty`.

- Right to cancel within 14 days of receipt, then 14 more days to return the goods. Full refund including delivery, on inspection. Customer pays return shipping (unless collected).
- Hygiene exclusions: haircare, electric blankets, headphones and Bluetooth headsets are returnable only if unopened in original packaging.
- Software activation key cards: silver strip must be intact.
- Warranty: product page shows the manufacturer warranty (e.g. "1 Year Parts and Labour", "2 Years Parts and Labour"). Statutory: Powercity is liable for non-conformity for at least 2 years from delivery.

## Price match

Source: `/groups/site/nofussprice` ("No Fuss Price Promise"). Quote: "If our Price is not right, simply click here to send us a link to a better in stock Republic of Ireland (ROI) based deal, and we will email you back a transaction with an improved offer. The items must be instock and available for immediate delivery." It is a request form. Don't submit it.

## Quirks and risks

- Site has an `ajax` add-to-cart (`/product/cart-add?id=...`). It is a write. Blocked.
- Session cookie and CSRF token are per visit. Don't store them.
- Some markup has stray `<!-- -->` comments and broken `<img>` tags. Parse text, not raw HTML.
- Cookie consent banner (`cookieconsent`): not required for content on plain HTTP.
