---
id: donedeal
name: DoneDeal
domains: [donedeal.ie]
kind: classifieds
categories: [everything]
urls:
  search: "https://www.donedeal.ie/all?words={query}&sort=publishdatedesc"
  check-search: "https://www.donedeal.ie/all?words=bike&area=Dublin&price_from=100&price_to=500&sort=publishdatedesc"
  check-product: "https://www.donedeal.ie/bicycles-for-sale/kids-bike/43151763"
defaults: { read: on, write: ask }
readPosts: []
blocked:
  - /accounts
  - /adwrite
  - /donedeal
  - /adview/api
  - /ratings
  - /remove
  - /update
  - /bump
  - /history-check
  - /car-finance
  - "*?userId*"
  - "*/contact*"
  - "*/message*"
  - "*/phone*"
  - "*/buynow*"
  - "*/checkout*"
concurrency: 2
---

# DoneDeal (donedeal.ie)

Irish classifieds (cars, farming, general). Next.js (Pages router), currency EUR, used items by default, mostly private sellers. All listing data is server-rendered into `__NEXT_DATA__`; no JSON API is needed. Plain HTTP with a browser User-Agent works (no challenge seen). `robots.txt` disallows `/accounts/`, `/adview/api/`, `/ratings/`, `/adwrite/*`, `*?userId`.

Contacting sellers is not allowed: no messages, no "reveal phone", no Buy Now, no deposit, no saving ads, no reporting. Give the user the ad link (`open`) and stop. `publisherPhoneEnc` and `seller.name` are never to be copied.

## URL patterns

- Search, all sections: `https://www.donedeal.ie/all?words={query}&area=Dublin&price_from=100&price_to=500&sort=publishdatedesc&start=30`
  - `area=<County>` (`Dublin`, `Kildare`, ... see `countyList[].value`). Repeat the param for several: `area=Kildare&area=Dublin` (verified: counts add up, 128 + 601 = 729).
  - `price_from`, `price_to` (euro integers). `sort`: `` (best match), `publishdatedesc` (newest), `publishdateasc`, `priceasc`, `pricedesc`, `sellerratingdesc`.
  - Paging: `start=<offset>` with page size 30 (`paging.nextFrom`). `from=` does nothing. `radius` has no effect without a town/coordinates, so filter by county and compute distance yourself from `geoPoint`/town.
  - `words` is a loose keyword match across all sections (e.g. "bike" also returns motorbikes); add a category path or more keywords to narrow.
- Section pages, e.g. `/bicycles-for-sale`, `/cars`, `/farming`: a different layout. `/bicycles-for-sale/Kildare?...` returned 404, so stay with `/all?...&area=` unless you verify a section URL first.
- Ad: `/<section>-for-sale/<slug>/<id>` (id is numeric; the slug is cosmetic). Ads carry `?campaign=3` when spotlighted; drop it.

## Adapter

`sites/donedeal.ts`: search and offer (no pickup, no reviews). Reads `__NEXT_DATA__` of `/all?words=…` (price range native; up to 3 pages of 30) and of an ad page (`/cars-for-sale/ad/<id>` serves any section's ad, `/all/` does not). Offers carry `seller` (private, trader or unknown), `condition` (the ad's stated one, else used), `details.Location` and `details.Posted`. Prices come from `priceInEuro`; `priceOnRequest` ads are skipped; token prices (EUR 1, EUR 10) on expensive items are unreliable. Ad pages: `state` other than "Published" means gone. Never read `seller.name`, `publisherName`, `publisherPhoneEnc`, `userId`. The `page` tool's markdown drops prices, so use the adapter for numbers.

## Used goods and scams

All used unless stated. Age: ads older than a few weeks are often sold; flag them. Ads have no buyer protection or returns; DoneDeal "Buy Now" (Stripe) exists on some ads and is a purchase: blocked. Flag: price far below market, no location, off-platform payment, requests to move to WhatsApp. Seller verification flags (`email`, `phone`) are weak evidence only.

## Location

Counties only (plus town sometimes). User: Maynooth 53.381, -6.591 (Co. Kildare, on the Dublin border). Search `area=Kildare&area=Dublin`, then rank by the ad `geoPoint` (ad page) or town. Search rows have no coordinates, so distance from rows is county-level.

## Policy and help

Help centre: https://donedeal.ie/hc/en-us (not fetched here). No delivery, returns or price match from DoneDeal itself: terms are the seller's. Unknown: Buy Now delivery terms.

## Quirks

- `searchMatches` and `paging.totalResults` may differ slightly.
- `__NEXT_DATA__` holds `userId`s and ad analytics; keep out of notes.
- Rate limit unknown; keep `concurrency: 2`.
