---
id: bing
name: Bing
domains: [bing.com]
kind: aggregator
categories: [everything]
urls:
  shop: "https://www.bing.com/shop?q={query}&cc={cc}&setlang={lang}"
  check-shop: "https://www.bing.com/shop?q=kreg+pocket+hole+jig&cc=IE&setlang=en"
defaults:
  read: on
  write: ask
concurrency: 1
blocked:
  - "/rewards*"
  - "/account*"
  - "/profile*"
  - "/fd/auth*"
  - "/secure*"
  - "/ck/a*"
  - "/aclick*"
  - "/search/history*"
  - "/profile/history*"
---

# Bing Shop (bing.com/shop)

Aggregator: results are leads, not facts. Adapter `sites/bing.ts` supports search and offer (no pickup, no reviews). Only Bing Shop is covered; Bing web search isn't onboarded.

> **Warning: shopping-aggregator prices and availability are often stale or wrong.** Use Bing Shop only to see which shops sell a product, then verify price, stock and delivery on the merchant's own site (its site doc, if onboarded) before reporting anything.

## Offers

- `url` is the merchant's product page (Bing's ad-click redirect unwrapped, tracking params dropped); `seller` is the merchant. Condition is `unknown` (Bing doesn't show it), delivery is `unknown` unless the card says "Free shipping"
- Most cards are sponsored (merchant-paid placements): `details.Sponsored`. They're still real offers
- `id` is `<Bing offerid>:<query>`. Bing has no product page, so `offer` re-runs the search and fails with "re-check on the merchant's site" when the listing has dropped out of the results
- A search returns about 20 cards; the order changes between loads

## Transport and region

- Plain HTTP returns a page that doesn't match the query: the adapter uses guarded document navigation for Shop searches
- Shop is fixed to `cc=IE&setlang=en` (euro prices, Irish merchants). `cc=GB` gave £ prices and UK sellers (eBay UK first); other region codes are untested. Check the currency symbol matches the region you want
- Not onboarded merchants: read their pages with `page` (read-only) or open the merchant's own site doc

## Rules

- 1 request at a time, use the cache, at most a few searches per task
- Stop on any challenge page and tell the user. Never solve it
- Never sign in, never touch Microsoft account or Rewards pages
- Never open `/aclick` or `/ck/a` links: the adapter unwraps them to the merchant's URL
