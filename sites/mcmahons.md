---
id: mcmahons
name: McMahons Builders Providers
domains: [mcmahons.ie]
kind: retailer
categories: [home-improvement]
urls:
  search: "https://www.mcmahons.ie/catalogsearch/result/?q={query}"
  delivery: "https://www.mcmahons.ie/delivery-information"
  terms: "https://www.mcmahons.ie/terms-and-conditions"
  branches: "https://www.mcmahons.ie/contact-us"
  check-search: "https://www.mcmahons.ie/catalogsearch/result/?q=drill"
  check-product: "https://www.mcmahons.ie/dewalt-sds-hammer-drill-240v-26mm-3-mode-53726"
defaults: { read: on, write: ask }
blocked: ["/checkout", "/customer/account", "/wishlist", "/review", "/sendfriend", "*/checkout*", "*/customer/account*", "*/wishlist*"]
concurrency: 2
---
McMahons (mcmahons.ie), Irish builders' merchant (Magento 2, Cloudflare; plain HTTP gets a 403 challenge, a browser tab passes). Prices are EUR and include VAT (ex-VAT is in the same price box). Trade prices need a login: show retail prices and say so. Ships to the island of Ireland only.

## Adapter

`sites/mcmahons.ts`: search, offer (by code or URL) and pickup. Offer ids are product codes (`53726`, the last slug segment). Search matches words loosely; no reviews. Pickup reads the product page, which embeds every branch with quantity and coordinates (same Magento component as TJ O'Mahony), so no store list or branch selection is needed.

## URL patterns

- Search: `https://www.mcmahons.ie/catalogsearch/result/?q={query}` (12 per page; an exact code match redirects to the product).
- Product: `https://www.mcmahons.ie/<slug>-<code>`. Departments: `/building-materials`, `/timber-sheet-material`, `/screws-nails-fixings`, `/plumbing-heating`, `/doors-floors-joinery`, `/garden-landscaping`, `/sale`, `/search-by-brand`.
- Always use the `www` host.

## Delivery (source: https://www.mcmahons.ie/delivery-information)

- Courier (under 25 kg and 2 m, island of Ireland): €15, next working day if ordered before noon, else two. Free over €500 ex VAT.
- McMahons Truck (over 25 kg): €25, within four working days, only in listed Eircode areas around branches. Free over €500 ex VAT.
- The product JSON-LD says €9.99; the delivery page says €15, so the adapter uses the page. Heavy items cost more than the default.

## Collection and branches

Pickup quantities come from the product page. Branches (lat, lon): Cherry Orchard Dublin 53.344945, -6.371327; Limerick 52.655724, -8.652684; Cork 51.902388, -8.450279; Fermoy 52.148976, -8.281519; Carrigaline 51.811725, -8.383155; Dunmanway 51.716929, -9.112489; Tralee 52.273388, -9.679972; Galway 53.301007, -9.007172; Portarlington 53.167198, -7.187928; Longford (Brian Fallon Hardware) 53.716447, -7.738078. Contact page lists more (Northern Ireland, hardwood centres).

## Returns

Goods may be returned within 30 days of delivery by prior arrangement only, credited at the seller's discretion, with a 15% handling charge. Damage and defect claims within three working days. Source: https://www.mcmahons.ie/terms-and-conditions

## Quirks

- Cloudflare challenge: `fetch`/curl get 403; the guarded browser tab passes it.
- Search cards show no stock; use `pickup`.
