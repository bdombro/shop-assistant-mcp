---
id: tjomahony
name: TJ O'Mahony
domains: [tjomahony.ie]
kind: retailer
categories: [home-improvement]
urls:
  search: "https://tjomahony.ie/catalogsearch/result/?q={query}"
  search-page: "https://tjomahony.ie/catalogsearch/result/index/?p={page}&q={query}"
  check-search: "https://tjomahony.ie/catalogsearch/result/?q=drill"
  check-product: "https://tjomahony.ie/makita-da4000lr-110v-13mm-angle-drill-56makda4000lrl.html"
  branches: "https://tjomahony.ie/branch-finder"
  delivery: "https://tjomahony.ie/delivery/"
  returns: "https://tjomahony.ie/returns-refunds-policy"
defaults: { read: on, write: ask }
blocked: ["/checkout", "/customer/account", "/wishlist", "/review", "/sendfriend", "*/checkout*", "*/customer/account*", "*/wishlist*"]
concurrency: 2
---
TJ O'Mahony (tjomahony.ie), Irish builders' providers and DIY retailer (Magento 2, Cloudflare). Prices are in EUR. Shown prices include VAT (`price-including-tax`). Ex-VAT is in the same price box. Delivery is Republic of Ireland only. Not Northern Ireland, not international. Handy Hardware and PH Ross (bathrooms, HPC insulation) are sister brands under the same storefront.

## Access rules

- Plain HTTP works with a browser User-Agent. No browser is needed for search, product, branch or policy pages.
- `robots.txt` has `Disallow: /catalogsearch/` and `Disallow: /catalog/product/view/`. Search pages still answer over HTTP. The product URLs are not disallowed. Treat search as a user-initiated read at low concurrency (see the recon file, open question 1).
- Cart, checkout and customer pages are blocked above. Do not add to basket. Do not sign in. Trade accounts show "Log in for trade prices". Trade prices are behind login, so do not try to get them.
- Magento GraphQL (`/graphql`) returns 403 from Cloudflare to plain HTTP. Do not rely on it.

## Adapter

`sites/tjomahony.ts`: search, offer (by SKU or URL) and pickup. Pickup reads the product page: it embeds every branch with its quantity and coordinates, so no `stores` list or branch selection is needed. Out-of-stock items have no collection button but the list is still embedded. Search matches words loosely and returns related items; offer ids are SKUs like `56DEWDCD996P2` (the search finds them). Reviews: none (Trustpilot is shop-wide only).

## URL patterns

- Search: `https://tjomahony.ie/catalogsearch/result/?q={query}` (spaces as `+`). Page 2: `…/catalogsearch/result/index/?p=2&q={query}`. Pagination links are in the HTML. "Did you mean" and "Related search terms" appear under the list. The `q=drill` check returns 788 results, 36 per page (`toolbar-amount`).
- Product: `https://tjomahony.ie/<slug>-<sku-ish>.html`. Slugs come from the search results.
- Department and category listings are not in the sampled output. They are not tested.
- Sort and filter query params (`product_list_order`, `product_list_limit`, `filter_*`, `price`) are disallowed by `robots.txt`. Use the `q` search only.
- Branch finder: `https://tjomahony.ie/branch-finder`.
- Homepage: `https://tjomahony.ie/`. `https://www.tjomahony.ie/` returns 302 to `https://tjomahony.ie/`. Always use the apex domain.

## Page reads

- Product page: JSON-LD `Product` has name, SKU, price and availability. "This product is out of stock and not available to order right now" marks unavailable items. Prices on the page are retail inc. VAT, with ex-VAT beside them.
- Search cards carry no stock. "0 Available for collection" on a product page means nothing without a branch: use `pickup` (per-branch quantities).

## Delivery (source: https://tjomahony.ie/delivery/, last update 18 Aug 2026)

- Small items (Republic of Ireland): €9.99, 3–4 working days.
- Large and heavy (standard delivery area only): €29.99, 5–7 working days.
- Extra large (standard area only): €50.00, 5–7 working days.
- Paint 5 L or less (standard area): €19.99, 5–7 working days.
- Free delivery on eligible small and large/heavy orders over €1,499, standard area only. Not for restricted areas or extra large.
- Catalogue (special order) products add lead time, marked on the product page.
- Not available to Northern Ireland or outside Ireland.

## Collection (Click and Collect)

- Free collection from a chosen branch, for items in stock at that branch. Source: delivery policy page and product page text ("FREE Click & Collect is available for items in stock at your selected branch").
- Branch list: https://tjomahony.ie/branch-finder (16 branches; the product page embeds the same list with stock).
- Dublin branches (lat, lon), from the branch finder:
  - TJ O'Mahony Ballymount, Ballymount Cross, Dublin 24, D24 X39K: 53.311959, -6.357767. Closest to south-west Dublin.
  - HPC Insulation, M50 Business Park, Ballymount, Dublin 12: 53.309458, -6.351080
  - TJ O'Mahony Decking & Paving Centre Ballymount, Dublin D12 PK81: 53.315536, -6.347915
  - PH Ross Bluebell, JFK Rd, Dublin 12: 53.327345, -6.352623
  - TJ O'Mahony Glasnevin, Dublin 11: 53.375486, -6.300660
  - TJ O'Mahony Kilshane, Finglas, Dublin 11: 53.423686, -6.325072
  - PH Ross Cabra, Dublin 7: 53.357773, -6.288824. PH Ross Cabra showroom at Old Cabra Road, Dublin 7: 53.357903, -6.288715
- Airport: no branch found in the finder. Not verified against the store locator beyond this list.

## Returns and warranty

- Refund or exchange within 30 days of delivery or collection, if unused and in re-saleable condition with all parts and accessories. Source: https://tjomahony.ie/returns-refunds-policy. Statutory rights are not affected.
- Warranty: not checked. Tools often carry a manufacturer warranty (see category notes).

## Price match

- Not checked. Not in the pages sampled.

## Quirks

- The `www` host 302s to the apex. Use the apex.
- Trade prices and the trade account sign-in are behind a login. Show retail prices, and say they are retail.
- The Cloudflare "Attention Required" page is returned to `/graphql` on plain HTTP. It is not a site-wide block for HTML pages.
- Some products are "Catalogue" (special order) with extra lead time, and some are out of stock with no order option.
