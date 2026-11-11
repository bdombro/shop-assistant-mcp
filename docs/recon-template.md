# Recon: <site id>

Fill every field. Write `unknown` where you couldn't tell. Don't ask the user. Don't buy, sign in, or change settings. Stop on a CAPTCHA and record it.

## Fingerprint
- Platform (Shopify, WooCommerce, Magento, PrestaShop, BigCommerce, Squarespace, Wix, SFCC, Next, Nuxt, other):
- Hosted search (Algolia, Klevu, Doofinder, Searchspring, Constructor, Bloomreach, none):
- Evidence (headers, script URLs, globals):

## Data source per page type
Reads always run in the browser. For each, give the cheapest source: a JSON endpoint the site calls (method, URL, body), JSON-LD, or HTML selectors, and whether it needs a page load.
- search:
- product:
- reviews:
- offers:
- stock:
- departments:
- policy:

## URLs
- Search template (with `{query}`):
- Page 2 and sort:
- Refinements (price, brand, availability):
- Product URL shape:
- Reviews and critical reviews:
- Offers or other sellers:
- Department list:
- Store stock or store finder:
- Sample URLs for `check` (1 search, 1 product). Record them in the site doc's frontmatter `urls` as `check-search` and `check-product` (the key after `check-` is the page kind):

## Data
- Where listing rows live (selector, `__NEXT_DATA__` path, JSON-LD, API URL):
- Fields and where each comes from: title, price, was price, currency, unit price, stock, delivery, seller, condition, rating, review count
- Price format quirks (e.g. `€ 26 .95 INC VAT`; ex-VAT):
- Stock: online / per store, and whether a selected store is needed:
- Seller and condition (marketplace, used):
- Critical reviews: how to get 1-3 star:

## Policy
Source URL for each. Quote the terms.
- Delivery (cost, thresholds, cutoffs, speed):
- Collection and stores (name, address, lat/lon):
- Returns and warranty:
- Price match:

## Capabilities
Numbered, with proposed policy. Mark what the site offers, not what we'd do.
1. search: read
2. product page: read
3. reviews: read
4. view basket: read
5. add to basket: write, ask
6. orders and account: forbidden
7. checkout and payment: forbidden
8. other (store select, wishlist, messaging):

Proposed `defaults`, `readPosts` (same-site POSTs that only read), `blocked`, `allowedPages`, `concurrency`:

## Quirks and risks
- Bot protection, redirects, consent walls, rate limits:
- WebSocket writes:
- Anything else a reader needs:

## Fixtures
Saved paths: one search page, one product page.
