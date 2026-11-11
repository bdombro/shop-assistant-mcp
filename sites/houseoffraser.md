---
id: houseoffraser
name: House of Fraser Ireland (FRASERS)
domains: [houseoffraser.ie]
kind: retailer
categories: [everything]
urls:
  search: "https://www.houseoffraser.ie/searchresults?descriptionfilter={query}"
  check-search: "https://www.houseoffraser.ie/searchresults?descriptionfilter=bath%20towel"
  check-product: "https://www.houseoffraser.ie/brand/boss-home/plain-bath-towel-856351"
  delivery: https://help.houseoffraser.ie/hc/articles/6018264939551-4-delivery
  terms: https://help.houseoffraser.ie/hc/articles/6017085194783-terms-and-conditions-all-sections
defaults: { read: on, write: ask }
readPosts: []
blocked:
  - /cart
  - /checkout
  - /login
  - /accountinformation
  - /myaccount
  - /wishlist
  - "*/checkout*"
  - "*/account*"
  - "*/wishlist*"
concurrency: 2
---

# House of Fraser Ireland (houseoffraser.ie)

Frasers Group department store, now titled "FRASERS Ireland" on its pages. Fashion, beauty, home and furniture, gifts, plus a long tail of Frasers Group catalogue (electronics, toys, sports). Currency EUR, prices incl. VAT. Images load from `houseoffraser.co.uk`. Online only in Ireland: `/stores` lists no Irish stores, so no pickup.

Adapter: search and offer (one offer per colour, `family` returns every colour; sizes with prices and stock in `details.Sizes`). No reviews: the site shows none.

## URL patterns

- Search: `/searchresults?descriptionfilter={query}`, 60 rows a page, page 2 with `&dcp=2`. Matching is very loose: nonsense queries still list ~100 unrelated products, so check names.
- Product: `/brand/<brand>/<slug>-<6-digit product id>#colcode=<8-digit colour id>`. The fragment picks the colour; `?colcode=` does not. Offer ids are colour ids.
- Searching a colour id or product id lists that product first.

## Delivery and returns

- Product pages: Standard Delivery €6.99, "within 3 - 7 days (excludes Public holidays)". Free-delivery threshold not seen.
- Help pages (`help.houseoffraser.ie`) redirected to a sign-in in recon on 2026-11-01; returns terms not captured.

## Quirks

- Akamai Bot Manager: sensor POSTs to `/akam/13/pixel_*` and a random `/rx9Vfz/...` path. Reads worked without them in recon.
- `/api/productlist/v1/getforcategory` (needs `portalSiteId=343`) returned zero products for search terms; the HTML search is the source.
