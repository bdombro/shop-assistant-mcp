---
id: mydiy
name: MyDIY.ie
domains: [mydiy.ie]
kind: retailer
categories: [home-improvement]
urls:
  check-other: https://www.mydiy.ie/
  check-product: https://www.mydiy.ie/products/kunys-sg-5023-cordless-drill-holster.html?F1=0&ID=167184
  delivery: https://www.mydiy.ie/delivery-information.htm
  returns: https://www.mydiy.ie/returns.htm
  contact: https://www.mydiy.ie/contact-us.htm
defaults:
  read: on
  write: ask
readPosts: []
blocked:
  - /Search/
  - /Subscribe/
allowedPages: []
concurrency: 2
---

# MyDIY.ie (mydiy.ie)

Irish hardware retailer (MyDIY Ltd, owned by JG Engineering Supplies Group). Currency EUR. Hosted search by Doofinder (zone `eu1`).

Adapter `sites/mydiy.ts`: `search` and `offer` (delivery only; no branch stock, no reviews). Search is the Doofinder hosted API called from a mydiy.ie tab (403 without the site's Origin). The site's own search box POSTs to `/Search/`, which 302s home; don't use it. Ids are the SKU (the `ID` URL param); the page's `PID` is the product group. If the Doofinder hashid (`ad68c82e…`) rots, take it from the layer config for installation `17c4f79b-ad37-43c1-81d3-91d531cba6f9` (zone `eu1`).

## Product pages

- `/products/<slug>.html?F1=0&ID=<id>`, plain HTTP with a browser User-Agent. Prices inc VAT with ex-VAT and a "List:" was-price on reductions; "In Stock" / "Out of Stock" text. No JSON-LD, spec table or review service. Titles from search carry mojibake (`Â`), stripped by the adapter.

## Delivery, collection, returns (source URLs)

- Delivery (https://www.mydiy.ie/delivery-information.htm): DPD. Each product has an estimated delivery time ("next-day", "3-day"). An order arrives on its slowest item's time. Most orders ship in 1-2 days, some 4-5 working days. Orders placed before 2 PM usually ship within 24h. Orders after 1:30 PM Friday or at weekends ship the following week.
- Charges: free delivery over EUR 150 within Ireland, else EUR 8.99. International by weight, quote on request.
- Someone must sign for delivery. Two failed attempts can return the order.
- Damage or shortage: report within 24h (damage) or 3 days (shortages).
- Returns (https://www.mydiy.ie/returns.htm): money-back within 16 days of delivery if unused, in original condition and packaging. Refund within 72h of receipt, excluding shipping. A returns number is required and must be on the outside of the parcel.
- Price match: no page found in the crawl. Treat as unknown.
- Click and collect: not found in the crawl. The only public pickup point is the warehouse at Unit 20 Goldenbridge Ind. Est., Inchicore, Dublin 8 (https://www.mydiy.ie/contact-us.htm). Lat/lon approximate: 53.3430, -6.3260 (not geocoded; verify before relying on it). This is the closest to west Dublin. Tel +353-1-4406692. No store finder was found. Straight-line distance from Inchicore to Maynooth (53.381, -6.591) is roughly 18 km (rough calculation, not a road distance).

## Quirks

- Plain HTTP 302s: `http://mydiy.ie/` 301s to `https://www.mydiy.ie/`. `https://mydiy.ie/` returns 200 with no redirect. `/Search/` (GET and POST) 302s to the homepage. The brief said the homepage 302s. The first hop is 301 in the test. Check the redirect code before relying on it.
- The site sets `ShopperID` and `ASPSESSIONID` cookies. None are needed for read.
- Cookie consent banner ("I Accept") and a newsletter popup appear in the page text. They are chrome, not content.
- Pages are ASP.NET on IIS. A browser UA is needed.
