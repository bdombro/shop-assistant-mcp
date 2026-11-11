---
id: ebay
name: eBay
domains: [ebay.ie, ebay.co.uk]
kind: marketplace
categories: [everything]
urls:
  search: https://www.ebay.ie/sch/i.html?_nkw={query}
  check-search: https://www.ebay.ie/sch/i.html?_nkw=cordless+drill&LH_BIN=1&_sop=15
  check-product: https://www.ebay.ie/itm/315247246979
defaults:
  read: on
  write: ask
readPosts: []
blocked:
  - /bfl/*
  - /bid/*
  - /sell/*
  - /myb/*
  - /mys/*
  - /mesgmy/*
  - /cart/*
  - /vod/*
  - /sc/*
  - /usr/*/contact*
  - /itm/*/contact*
  - "*/offer/*"
  - "*/makeoffer*"
  - "*/ws/eBayISAPI.dll?*"
  - "*signin.ebay.*"
  - "*pay.ebay.*"
  - "*checkout.ebay.*"
  - "*cart.payments.ebay.*"
  - "*offer.ebay.*"
  - "*contact.ebay.*"
  - "*bid.ebay.*"
allowedPages: []
concurrency: 2
---

# eBay (ebay.ie, ebay.co.uk)

Marketplace: auctions and Buy It Now from private and business sellers worldwide. No keys. Plain HTTP (even with a browser User-Agent) returns a 403 "Error Page | eBay" (Akamai/bot filter), so every page kind needs the browser transport (the user's Brave). A Brave tab loaded normally with no CAPTCHA.

## Stores and currency

- `www.ebay.ie`: the default. Search rows are in EUR ("EUR 10.49"). Item pages can still show the seller's listing currency, e.g. "£7.20", while the JSON-LD `offers` says EUR (8.52). Search EUR and item-page EUR were not equal for the same item (EUR 10.49 vs 8.52): treat the search price as the one eBay shows Irish buyers and re-read the item page before quoting a total.
- `www.ebay.co.uk`: GBP. Included in `domains` so links from ebay.ie listings and UK-listed items can be opened and read. Prices are in GBP: convert to EUR, state the rate used and say it is converted. UK sellers ship to Ireland as an import from outside the EU: customs, import VAT and handling fees may apply (eBay's Global Shipping Programme or import charges collected at checkout, varies by listing). Say so for any UK or other non-EU item.
- Many sellers are in China and the UK. A row's "from <country>" label is the item location. Flag non-EU locations (UK, China, US) as customs/VAT risk and slow delivery. EU locations (Ireland, Germany, Poland...) have no customs.

## URL patterns

- Search: `https://www.ebay.ie/sch/i.html?_nkw={query}` (spaces as `+`).
- Item: `https://www.ebay.ie/itm/<item id>` (12 digits). Drop the tracking query (`_skw`, `itmprp`, `hash`).
- Category browse: `/b/<slug>/<id>`; breadcrumbs in item JSON-LD give them.
- Seller items: `/usr/<seller>` (read-only page; its "Message" and "Contact" links are blocked).

### Query syntax (verified `(a,b)` and `-word` on ebay.ie)

- Space between words = AND (every word must match).
- `(a,b)` = OR: `(cordless,battery) drill` returned 583 results.
- `-word` excludes: `drill -bit`. `-(a,b)` excludes several.
- `"exact phrase"` for a phrase. Not tested in this recon.
- `*` wildcard suffix (`dewalt*`). Not tested.
- Literal `AND`/`OR` words are not operators: use `(a,b)`. Not verified.
- Search title only: `&LH_TitleDesc=0` (default). Include description: `&LH_TitleDesc=1`.

### Filters (query params, all verified in a rendered search unless noted)

- Buy It Now only: `LH_BIN=1`. Auctions only: `LH_Auction=1`. Accepts offers: `LH_BO=1` (not tested).
- Condition: `LH_ItemCondition=1000` (New, verified). Common codes: 1000 New, 1500 New other, 2500 Seller refurbished, 3000 Used, 7000 For parts. `|` joins: `3000|2500`.
- Item location: `LH_PrefLoc=1` Ireland only (domestic, not tested), `LH_PrefLoc=3` European Union (verified: chip "European Union"), `LH_PrefLoc=2` worldwide (not tested). Specific country: `LH_LocatedIn=<id>` (not tested). Ships to Ireland: `LH_AvailTo=IE` (not tested). Note: Irish eBay already limits results to items that can ship to the user.
- Price: `_udlo=<min>&_udhi=<max>` (EUR on ebay.ie; `_udhi=100` verified, chip "Under EUR 100.00").
- Free postage: `LH_FS=1` (not tested).
- Sort `_sop`: 12 best match, 15 price + postage lowest first (verified), 16 price + postage highest, 2 lowest price, 10 newly listed, 1 ending soonest.
- Page: `_pgn=2`; items per page `_ipg=60` (240 max; not tested). Result count in `.srp-controls__count-heading` ("17,000+ results for cordless drill").

## Adapter

`sites/ebay.ts` supports search and offer (no pickup: online only; no reviews: eBay rates sellers, not items). Rows carry `seller`, `condition`, postage as `delivery` (source `listing`) and, in `details`, auction vs Buy It Now, bids, time left, item location, returns and seller feedback. Prices are in the listing currency as eBay shows Irish buyers (EUR on ebay.ie); item page text may show the seller's currency while the JSON-LD says EUR. A search row with a price range (variations) takes the low end and keeps the range in `details`. In the guarded tab search rows often carry no seller line (`seller: "unknown"`); `offer` always names the seller. Search and item prices can differ by a few cents (currency conversion): re-read with `offer` before quoting. Placeholder cards ("Shop on eBay", ids over 12 digits) are skipped. Item pages are 0.6-1.4 MB: read them through the adapter, not `page`. Seller names are public, but don't copy them into notes.

## Seller, condition, safety

- Seller type is in the subtitle (Business or Private). Seller feedback % and count matter more than item stars; low counts are weak evidence.
- Items from China are common with very low prices: often accessories, not the product searched for (see fixture: "drill handle", "carbon brushes", "trigger switch"). Check the title carefully, and the condition ("Brand new", "Pre-owned", "For parts").
- eBay Money Back Guarantee covers most purchases paid via eBay checkout; private-seller returns depend on the listing ("30 days return. Buyer pays for return postage" on the sampled item). Source: https://www.ebay.ie/help/buying/ (not fetched in this recon).
- Auctions: show current bid and end time; never bid.

## Blocked actions

Never place bids, Buy It Now, Add to basket, Watch, Make offer, Contact or Message seller, or open checkout, sign-in, account (`/myb`, `/mys`, `/mesgmy`) and sell pages. The item page has buttons "Buy It Now", "Add to basket", "Watchlist" and "Message": reading is fine, clicking is not. Give the user the item URL via `open` instead.

## Quirks

- `/.well-known/ucp` returns 302 to `https://pages.ebay.ie/.well-known/ucp`, which in turn returned 302 to `https://www.ebay.com/n/error`. No UCP endpoint: don't set `ucp`.
- Plain HTTP 403. Browser only.
- Result card links are `target=_blank`: use the URL, don't click.
- Prices on rows may be ranges (variations), "or Best Offer" is negotiable, not fixed.
- `LH_PrefLoc=3` filtered to EU; results still include "from Ireland" items.
