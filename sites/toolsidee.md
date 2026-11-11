---
id: toolsidee
name: Toolsidee
domains: [toolsidee.ie]
kind: retailer
categories: [home-improvement]
urls:
  search: "https://www.toolsidee.ie/zoeken/?searchData={query}"
  check-search: "https://www.toolsidee.ie/zoeken/?searchData=cordless+drill"
  check-product: "https://www.toolsidee.ie/product/scheppach-20v-battery-4-0-ah-sbp4-0-P20481.html"
  shipping: https://www.toolsidee.ie/pagina/verzendkosten/6
  vat-free: https://www.toolsidee.ie/pagina/btw-vrij-leveren/10
  returns: https://www.toolsidee.ie/pagina/retour-sturen/8
  payment: https://www.toolsidee.ie/pagina/betalen/5
defaults: { read: on, write: ask }
readPosts: []
blocked: ["/winkelwagen", "/added_succes.php", "/inloggen", "/registreren", "/pakket-volgen", "*/winkelwagen*", "*/added_succes*", "*/inloggen*", "*/registreren*"]
allowedPages: []
concurrency: 2
---

# Toolsidee (toolsidee.ie)

Dutch online tool shop (De Goudse Gereedschaphandel B.V., Waddinxveen, NL) with an Irish storefront on `.ie`. Sister stores on other domains (`.co.uk`, `.com`, `.de`, `.fr`, `.nl`-style languages) share the catalogue. This doc covers `.ie` only. Currency EUR. Ships from the Netherlands. No stores, so no pickup and no store stock. Cheap but slow: the warehouse is in NL and the advertised delivery is 1-3 working days across Europe, but bank transfer and freight add days and cost.

Stack: custom PHP 7 site behind Cloudflare, server-rendered HTML. No JSON API. Plain HTTP works with a desktop browser User-Agent (`fetch` without one may get blocked). Reads need no browser.

## URL patterns

- Search: `https://www.toolsidee.ie/zoeken/?searchData={query}`. The field name is `searchData`, not `q`. `?q=` returns a generic page with no results. Results are one page, about 17 products for "cordless drill". No pager was seen. Treat page 2 as unverified.
- Sort and brand filter: GET params `sorteer` (1 New, 3 Price high-low, 4 Price low-high, 5 Reviews) and `filter_brand` (brand id). Both trigger `onChange` submit in the UI. Param names come from the form markup. Not tested with HTTP.
- Category: `/<slug>_C<digits>.html`, e.g. `/electric-tools_C22_.html`, `/tool-trolley-black_C207_.html`, `/dewalt_C614_.html` (brand pages exist too).
- Product: `/product/<slug>-P<digits>.html`, e.g. `/product/scheppach-20v-battery-4-0-ah-sbp4-0-P20481.html`. The `P<digits>` id resolves the page. Listing links carry `?al_click=…&pos_r=…` tracking params. Strip them.
- Policy pages: `/pagina/<slug>/<id>`: `verzendkosten/6` (shipping), `btw-vrij-leveren/10` (VAT-free delivery), `retour-sturen/8` (returns and damage claims), `betalen/5` (payment), `over-ons/4`, `algemene-voorwaarden/3`.
- Language: site is English in the nav; some strings are Dutch. Only `.ie` pages were checked.

## Extraction hints

- Listing rows are server-rendered: `ul#product_list_show > li.product`. Per row:
  - link: `a[href^="/product/"]` (strip query)
  - title: `h3`
  - price: `.product_price_holder .amount` (text like `€ 60,95`, comma decimal). Use the JSON-LD on the product page for exact numbers.
  - was price: `.advies_prijs_klein s` (the "Regular price", only on reductions)
  - stock: `.op_voorraad` (`In stock`)
  - lead time: `.levertijd` (`Delivery time 1 - 3 working days`)
  - specs: `p.product_specs_l` (long text, can be stripped for listings)
  - rating: five `img` stars (`star_off.png` = unrated in the sample). Not a reliable number.
- Product page: `script[type="application/ld+json"]` with `@type: Product`. `offers.price` (dot decimal, e.g. `"36.95"`), `offers.priceCurrency` (`EUR`), `offers.availability` (`InStock`), `offers.sku`, `offers.hasMerchantReturnPolicy` (`applicableCountry: IE`, `merchantReturnDays: 14`), `aggregateRating` (`ratingValue` 8.2 of 10, `reviewCount` 1818, from Trustpilot's `toolsidee.com` profile).
- Product page text: `Delivery times 1 - 3 working days` in the sidebar. The `meta description` says `delivery 1-2 working days`. The two differ; record both.
- Nav and footer are heavy. Use `ul#product_list_show` / JSON-LD, not generic text, and add `strip` in future if markdown is noisy.

### Copyable recipe (read-only, for browser fallback)

```js
() => [...document.querySelectorAll("ul#product_list_show > li.product")].map((li) => ({
  title: li.querySelector("h3")?.textContent.trim(),
  price: li.querySelector(".product_price_holder .amount")?.textContent.trim(),
  was: li.querySelector(".advies_prijs_klein s")?.textContent.trim(),
  stock: li.querySelector(".op_voorraad")?.textContent.trim(),
  lead: li.querySelector(".levertijd")?.textContent.trim(),
  url: "https://www.toolsidee.ie" + li.querySelector('a[href^="/product/"]')?.getAttribute("href").split("?")[0],
}));
```

## Price format

- Listing: `€ 60,95` (comma decimal, space after €). Parse with the comma as decimal.
- JSON-LD: `"36.95"` (dot decimal). Prefer this on product pages.
- Prices are shown **incl. VAT** by default. A client-side toggle (`input.btw_widget`, "Incl. VAT" / "excl. VAT") switches display. It is a UI toggle, not a server state change, but don't click it: it's out of scope.
- The VAT rate for an Irish buyer is not stated on the site (see open questions). Don't assume 23% or 21%.

## Stock

- `.op_voorraad` / JSON-LD `availability` show warehouse stock in NL. No per-location number, no store, no store stock.
- "In stock" and "1 - 3 working days" are the only signals. Out of stock was not seen in the sample.

## Delivery to Ireland

Source: https://www.toolsidee.ie/pagina/verzendkosten/6 (shipping rates table, fetched 2026-10-14).

- Address delivery to Ireland: **€ 15.49** per order (the table's "Ireland" row). Parcelshop delivery is not offered for Ireland.
- The shipping cost shown "includes processing fees", per the page. "Free shipping on selected products" applies to items with a free-shipping icon on the product page. Free shipping thresholds were not stated. Check the basket (not done: read only).
- Orders over 30 kg: freight carrier, delivery only on Mon-Fri, truck with tail lift. "If you are not present and the goods cannot be delivered, double shipping costs will be charged." Irish freight rate not on the page; check the table or ask.
- Dispatch: "we aim to ship your order within 48 hours of receiving your payment." Bank transfer adds **3-5 working days** (source: https://www.toolsidee.ie/pagina/betalen/5).
- Advertised delivery time: "1 - 3 working days" (site-wide, in the header and product sidebar). Not Ireland-specific. Expect slower than that to Ireland from NL: dispatch plus carrier. Unverified; a real order test would be needed (out of scope here).

## Returns and warranty

Source: https://www.toolsidee.ie/pagina/retour-sturen/8 and JSON-LD on product pages.

- Reflection period: **14 days** (`merchantReturnDays: 14`, `applicableCountry: IE`). The footer widget also says "Reflection period 14 days".
- Damaged in transit: report within **7 days** of receipt, with photos (outside of package, opened package, inner packing, item, damage). Claim form first, then a return label. Claims after 7 days are not processed.
- Return shipping cost, who pays it, and warranty terms: not captured. Open question.

## VAT-free delivery

Source: https://www.toolsidee.ie/pagina/btw-vrij-leveren/10.

- VAT is removed only for business buyers in the EU with a valid VAT number that passes VIES, and the invoice and delivery address must match the VIES-registered address. No store or post-office deliveries.
- Private buyers pay VAT. Which VAT rate applies (Dutch 21% vs Irish 23%) is not stated on the policy pages.

## Payment

Source: https://www.toolsidee.ie/pagina/betalen/5. Bank transfer, iDEAL, Bancontact, SOFORT, Giropay. Card and PayPal were not listed in the captured text. Not verified further.

## Reviews

- Product pages show a Trustpilot aggregate in JSON-LD: 8.2/10 from 1,818 reviews (`uk.trustpilot.com/review/toolsidee.com`).
- The footer widget says "4.4 based on 85 reviews" (another Trustpilot profile in the page markup). The two sources disagree. Treat both as unverified.
- No on-site reviews were found in the HTML. There is no critical-review filter.

## Stores

None. Online only. The company address (Waddinxveen, NL) is not a pickup point.

## Quirks and risks

- Cloudflare is present (`cf-ray` DUB), no challenge seen with a browser UA. The response sets `PHPSESSID` (session cookie). The plugin does not need to keep it.
- `set-cookie` and `cache-control: no-store` on the homepage. The search and product pages are dynamic. Use a short cache.
- Add-to-cart opens `/added_succes.php?product_id=…` through a fancybox link (`a.add_to_cart`). It is blocked above.
- The `.ie` pages still load assets from `toolsidee.nl` (images).
- Mojibake: the `<meta>` says ISO-8859-1 while the body is UTF-8. Decode as UTF-8 to read `€` and accents.
