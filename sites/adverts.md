---
id: adverts
name: Adverts.ie
domains: [adverts.ie]
kind: classifieds
categories: [everything]
urls:
  search: https://www.adverts.ie/for-sale/q_{query}/list_view/
  search-county: https://www.adverts.ie/for-sale/q_{query}/county-Dublin/list_view/
  check-search: https://www.adverts.ie/for-sale/q_bicycle/county-Kildare/price_50-500/list_view/
  check-product: https://www.adverts.ie/hybrid-bikes/hybrid-bicycle-bike/38990546
  help: https://www.adverts.ie/help
defaults:
  read: on
  write: ask
readPosts: []
blocked:
  - /composepm.php
  - "*composepm.php*"
  - /offer.php
  - "*/offer.php*"
  - "*offer=*"
  - "*task=addToWatchList*"
  - "*task=removeFromWatchList*"
  - /make_comment.php
  - "*make-comment*"
  - "*make_comment*"
  - /reportad.php
  - /sendtofriend.php
  - /buynow
  - /sell
  - /login
  - /new-sign-in
  - /logout
  - /my-account
  - /myadverts
  - /member/*/feedback/leave*
allowedPages: []
strip:
  - "#mega_dropdown_menu"
  - "#search-form-area"
  - "#blur-bg"
concurrency: 2
---

# Adverts.ie

Irish classifieds (PHP, server-rendered, behind Cloudflare/CloudFront). Currency EUR. Used items by default: treat every ad as used unless the text says new. No login needed for reading. No JSON API.

Adapter: `search` (nationwide, optional min/max price) and `offer` (by ad id or URL). No `pickup` (collection is from the seller, location is in `details`), no `reviews`. Offers carry `seller`, `condition` (used unless the title or text says brand new, sealed, unused), `details.Location` and `details["Ad age"]`; delivery is `unknown`. Ads without a price above zero (free, swap, wanted, price on request) are left out of search; read them with `page`. Use `page` with the URL patterns below for county, category, seller-type or sort filters the adapter doesn't take.

## Search URLs

Path-based filters, in this order (extra segments combine). `{query}` goes after `q_`; use `+` or `%20` for spaces.

- Keyword, all categories: `https://www.adverts.ie/for-sale/q_bicycle/` (`/advertssearch.php?q=bicycle` redirects to it). `/for-sale/` is the sale section; other sections (`/jobs`, `/motors`, `/property-for-rent`) have their own roots.
- County: `.../county-Dublin/`, `.../county-Kildare/` (also Meath, Wicklow, Louth, ...). Town: `.../county-Kildare/area_Maynooth/` (area ids are the town names seen in rows, e.g. `area_Citywest`).
- Category: `/for-sale/sports-fitness/bikes/mountain-bikes/829/` (numeric id last; add `q_` and `county-` after it).
- Price (EUR, whole numbers): `price_50-250/`, `price_0/` (free only), `price_500-*/` (open ended).
- Seller type: `private_seller/`, `dealer_only/`. Ad type: `type_swap/`, `type_wanted/`, `type_buynow/`, `type_quicksale/`.
- Sort: `sortby_best_refresh_date-desc` (Most Recent), `sortby_price-asc`, `sortby_price-desc`, `sortby_start_date-desc` (Date Entered), `sortby_best_match-desc`.
- Page 2+: `.../page-2` (no trailing slash). `perpage-` exists.
- View: default is grid, which has no age or seller. Append `list_view/` to get age, seller name, description snippet and category. Always use `list_view/` for search.
- Example (Dublin, EUR 50-250, newest first): `https://www.adverts.ie/for-sale/q_bicycle/county-Dublin/price_50-250/sortby_best_refresh_date-desc/list_view/`
- Result count: `#sr-count` ("324 results"). Order of segments is forgiving; keep it as above.
- `https://www.adverts.ie/<adId>` redirects to the canonical ad URL.

robots.txt disallows `/q_`, `price_`, `area_`, `sortby`, `list_view`, `page-` combinations for crawlers. Use them sparingly: one search per user question, no crawling; the cache handles repeats.

## Reading rules

- Read only. Never use Send Message, Place offer (a GET form: `?offer=`), Watch Ad (GET `?task=addToWatchList&csrf_token=`), Leave Feedback, Report Ad, Make Comment, Buy Now / Quick Sale, Sell, or any phone-reveal / "Show number" control. All are in `blocked`, or are click actions the user does themselves
- To contact a seller, give the user the ad URL and tell them to use `open`; Adverts.ie messaging requires their own login
- Buyer protection: none for ordinary ads. "Buy Now" ads (`type_buynow`) use the site's checkout with some protection; do not open it. Scam signs: price far below market, off-platform payment, no location, urgent "courier" arrangements
- Ads that are sold, expired or removed: the page 404s or redirects to the category. Mark old ads (several weeks) as likely sold
- Dealers have no explicit label in rows: use the `dealer_only/` / `private_seller/` filters to separate them

## Distance (user: Maynooth 53.381, -6.591)

Ads give town and county only. Estimate from the town name: Kildare (Maynooth, Celbridge, Leixlip, Naas, Kilcock) is near; west/north Dublin is near; south Dublin (Dun Laoghaire, Bray, Wicklow) is 35-50 km. Ask for or compute lat/lon of a town when it matters. Use `county-Dublin/` and `county-Kildare/` as two searches.

## Delivery, returns, fees

None from the site: pickup or seller-arranged shipping is agreed between users. No returns, no warranty. Help: https://www.adverts.ie/help (not fetched in detail).

## Quirks

- Age is "entered or renewed", so it overstates freshness. `Date Entered` sort (`sortby_start_date-desc`) is the original post date
- 404 on `/all/q_x/`: the only global root for sale items is `/for-sale/`
- Condition, "no price", swap and wanted are inferred from text; ask the user before assuming
- Form-style equivalent of a filtered search: `advertssearch.php?search_cat=0&rs_min_price=50&rs_max_price=500&q=bicycle&status=active` (`status=active` hides sold/inactive)
