---
id: huntoffice
name: HuntOffice.ie
domains: [huntoffice.ie]
kind: retailer
categories: [everything]
urls:
  check-other: https://www.huntoffice.ie/
  check-product: https://www.huntoffice.ie/q-connect-box-of-copier-printing-paper-a4-80gsm-white-ream-kf01087
  delivery: https://www.huntoffice.ie/delivery
  returns: https://www.huntoffice.ie/returning-an-item
  contact: https://www.huntoffice.ie/contact-us
defaults:
  read: on
  write: ask
readPosts:
  # Loop54 hosted search, called cross-site by POST from the page and by the adapter.
  - "*huntoffice-ie.54proxy.com/search*"
  - "*huntoffice-ie.54proxy.com/getRelatedEntities*"
  # The Angular app refreshes its anonymous session token on every page load.
  - "*/api/core/v1/auth/refresh-token*"
blocked:
  - /cart
  - /checkout
  - /myaccount
  - /payment
allowedPages: []
concurrency: 2
---

# HuntOffice.ie (huntoffice.ie)

Irish office supplies, stationery, technology and furniture (HuntOffice Ltd, Dublin; sister site huntoffice.co.uk, not onboarded). Currency EUR. Angular SPA with server-rendered product pages. Hosted search by Loop54 (`huntoffice-ie.54proxy.com`).

Adapter `sites/huntoffice.ts`: `search` and `offer` (delivery only; no branch stock, reviews not built). Search is a POST to Loop54 from a huntoffice.ie tab (allowed via `readPosts`); any `user-id` header works. Ids are the numeric product id (SKU is `H<id>` for most items; the product page's JSON-LD `sku`). The adapter gives the inc VAT price.

## Prices: ex VAT by default

The site headlines prices **ex VAT** ("Price: €24.99 Ex VAT (€30.74 Inc VAT)", 23%), as a trade supplier does. Search rows carry both (`price` ex, `vatPrice` inc). Offers use `price.amount` inc VAT with `taxIncluded: true` and the ex VAT figure in `details["Price ex VAT"]`. The search `originalPrice` is a list/RRP figure, often far above the price; it is not used as a was-price. A VAT-registered business buyer pays the ex VAT figure.

## Search quirks

- Words match product text. Bare category words ("chair", "desk", "printer", "laptop") return no rows plus a redirect to a category page (`/office-chairs`); the adapter warns. Add a qualifier ("mesh chair", "standing desk frame") or a model code.
- Search by `H<id>` or the manufacturer reference ("KF01087") finds one product.
- Product pages: `/<slug>` (search rows end in `.html`; both resolve, the adapter drops `.html`). Category pages `/office-chairs`, `/paper` etc. are server-rendered.

## Delivery, returns (source URLs)

- Delivery (https://www.huntoffice.ie/delivery): ships from an Irish warehouse (product lines read "1-2 Days - In Stock Irish Warehouse"; other items 3-5, 5-7 or up to 20-30 days, shown per product). Majority of in-stock items next business day when ordered before 4:30pm. Standard charge EUR 9.99 for most items; bulky items cost more, shown in the cart. Remote locations may cost extra. Monday to Friday, 9am-5pm, signature required. Couriers deliver to ground floor only; furniture arrives flat-packed, assembly offered on chairs and desks. Report shortages within 48 hours. No free-delivery threshold found.
- Returns (https://www.huntoffice.ie/returning-an-item): consumers have 14 days from receipt, email returns@huntoffice.ie first. Business buyers have no cooling-off right; case by case with a possible restocking fee. Goods unused, in original packaging; buyer pays return carriage; refund excludes original delivery. Not returnable: opened technology, made-to-order or personalised items, food and drink, clothing and headsets.
- Showrooms for furniture in Dublin, Limerick, Cork and Kilkenny (not stock checkable; no pickup in the adapter).
- Price match: not found. Treat as unknown.

## Quirks

- Cart (`/cart`), account (`/myaccount`) and checkout are blocked. The page also talks to Firebase and Google reCAPTCHA in the background; these are not content.
- Cookie banner ("Accept / Decline / Customise") appears in page text.
