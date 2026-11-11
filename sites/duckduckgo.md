---
id: duckduckgo
name: DuckDuckGo
domains: [duckduckgo.com, links.duckduckgo.com]
kind: aggregator
categories: [everything]
urls:
  shopping: "https://duckduckgo.com/?q={query}&ia=shopping&iax=shopping"
  check-shopping: "https://duckduckgo.com/?q=kreg+pocket+hole+jig&ia=shopping&iax=shopping"
defaults:
  read: on
  write: ask
concurrency: 1
blocked:
  - "/settings*"
  - "/y.js*"
  - "/l/*"
---

# DuckDuckGo Shopping (duckduckgo.com)

Shopping aggregator over Bing Shopping: results are leads, not facts. Web search is not supported here. Adapter: `search` and `offer` (delivery only, no reviews or pickup). Every offer links off-site to the merchant (`url`), named in `seller`; prices are Bing's listing claims.

> **Warning: search-engine shopping results often misreport price and availability.** Use them only as hints for which shops to check, then verify price, stock and delivery on the merchant's own site doc before reporting anything.

## Shopping tab

- URL: `https://duckduckgo.com/?q={query}&ia=shopping&iax=shopping` (no challenge, signed out)
- Data: `links.duckduckgo.com/m.js?shopping=1&o=json&q=<query>&vqd=<token>` returns JSON (30 rows a page, `next` for more). `vqd` is in any duckduckgo.com page. Card links are trackers: the adapter unwraps them to the merchant URL; never open them yourself
- Region is ignored: results follow IP/Bing, not `kl` (`kl=uk-en` still returned Irish sellers in euro). For another country's market use Bing Shop (`cc=`)
- `price` is the struck-through earlier price when `low_price` is set; the adapter sets `price.was`
- Sellers are a mix of local shops (Toolsidee IE, Strahan Timber), resellers (Ubuy) and marketplaces (AliExpress, Temu, Alibaba); many rows are not the product searched. Treat everything as possibly paid
- Delivery is `unknown`. There is no product page: `offer` takes the merchant URL (the row `id`) and searches again; if the row is gone it fails with "re-check on the merchant's site"

## Rules

- 1 request at a time, use the cache, at most a few searches per task
- Stop on any challenge page and tell the user. Never solve it
- No account, no settings changes
- Result links to non-onboarded sites: plain-HTTP `page`, read-only
