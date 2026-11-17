# shop-assistant-mcp

```
      ______________
     /  .--------.  \
    /  /  SHOP   /|  |
   |  /--------/ |  |
   | |  basket | | /
   |_|________|_|/
```

Use this MCP to shop across 22 onboarded Irish, UK, and EU retail sites: compare offers, inspect product details and reviews, and check local stock.

The MCP uses your own Brave browser for interactive pages, but keeps shopping guarded: it works only with managed tabs, blocks checkout and account pages, and requires confirmation for write actions. It cannot place an order.

> **Client compatibility:** MCP **2026-07-28 only**; legacy clients cannot connect.
> The workspace has been exercised with OpenCode v2.2.6 using explicit July
> configuration. Cursor's July support is deferred; Claude Code July runtime
> acceptance is not yet verified. Shipping a plugin manifest is not proof of
> client compatibility.

## Try It

After setup, ask your agent:

> Compare 18V drills under EUR 150 from the supported Irish DIY shops.

## Features

- Search and compare offers, product specs, reviews, delivery, and stock across onboarded stores.
- Read product pages and use guarded browser tabs for pages that need interaction.
- Checkout, payment, login, and account pages are blocked; browser writes require confirmation.
- Small footprint: a standalone MCP binary reuses your existing Brave browser rather than bundling one.

## Install

```bash
brew tap bdombro/shop-assistant-mcp https://github.com/bdombro/shop-assistant-mcp
brew install bdombro/shop-assistant-mcp/shop-assistant-mcp
```

Note: check [releases](https://github.com/bdombro/shop-assistant-mcp/releases) for published binaries before running `brew install`; if no release is published yet, build from source per [contributing](docs/contributing.md). Installing a Cursor/Claude manifest only configures an endpoint; it does not install or start the native service.


## Configure the Shopping Browser

Install Brave with `brew install --cask brave-browser`. Interactive tools
require a Brave instance exposing a **loopback-only** CDP endpoint, normally
`http://127.0.0.1:9222`. Use a dedicated browser profile, not your everyday
signed-in profile; do not expose remote debugging to your network.

Set `SHOP_ASSISTANT_CDP_URL` if using another endpoint. Configure site
preferences in `$(brew --prefix)/etc/shop-assistant-mcp/config.json` (or
`SHOP_ASSISTANT_CONFIG`). Service environment variables can be placed in
`$(brew --prefix)/etc/shop-assistant-mcp/env` (`chmod 600`).
The server manages its own tabs and refuses checkout/account pages.
macOS supplies the confirmation dialogs; unattended mode denies prompts.


## Start and Manage the Service

```bash
brew services start shop-assistant-mcp
brew services info shop-assistant-mcp
brew services restart shop-assistant-mcp
brew services stop shop-assistant-mcp
```

The HTTP server binds `127.0.0.1:10005` (`http://127.0.0.1:10005/mcp`; pass `--host 0.0.0.0` to expose)
and writes rotated logs to `$(brew --prefix)/var/log/shop-assistant-mcp.log`
(rotates to `shop-assistant-mcp.log.1` at 10 MiB). Data lives in
`$(brew --prefix)/var/shop-assistant-mcp/`. To run in the foreground instead,
use `shop-assistant-mcp http` (or `./build/bin/shop-assistant-mcp http` from a
source checkout). Do not run the foreground server and service on the same port.


## Connect Your Agent

After starting the service, add this server to OpenCode v2.2.6's
`opencode.jsonc` (merge it with any existing servers):

```json
{
  "mcp": {
    "servers": {
      "shop-assistant-mcp": {
        "type": "remote",
        "url": "http://127.0.0.1:10005/mcp",
        "protocol": "2026-07-28"
      }
    }
  }
}
```

Use either an explicit server entry or a plugin manifest, not both.
Alternative client schemas are in the
[contributing guide](docs/contributing.md). The compatibility notice above
applies to every transport.


## Tools & HTTP Endpoints

Every tool is exposed over MCP (`shop-assistant-mcp http` on `http://127.0.0.1:10005/mcp`) and over HTTP at `/tools/<name>` (enabled by default). Existing `shop-assistant://` resources and data paths are unchanged.

| Tool | Mutating | Description |
| --- | --- | --- |
| `search` | No | Search one or more onboarded sites (or a category theme like `"diy"`, `"electronics"`) concurrently, returning normalized offers (`title`, `price`, `currency`, `url`, `stock`, `delivery`, `rating`, `specs`) filtered by `minPrice`, `maxPrice`, `inStockOnly`, and token budget. |
| `offer` | No | Fetch structured product details (`ProductPage`: price, stock, specs table, description, images, delivery options) from a product URL using its site adapter or JSON-LD. |
| `reviews` | No | Extract customer review summaries and individual reviews (`rating`, `title`, `body`, `date`, `verified`) for a product URL. |
| `page` | No | Read any allowed URL on an onboarded site as clean, sectioned Markdown (`section`, `maxTokens`), served from `shop.db` cache when fresh. |
| `themes` | No | List onboarded sites grouped by category theme, along with their current `read`/`write` policy and health status. |
| `open` | Yes | Open a new guarded browser tab on an onboarded site URL and return its `tabId` and accessibility/Markdown snapshot. |
| `navigate` | Yes | Navigate an existing guarded `tabId` to a new allowed URL. |
| `snapshot` | No | Capture the current accessibility tree / interactive element refs of a guarded `tabId`. |
| `screen` | No | Read the current rendered text/Markdown content of a guarded `tabId`. |
| `screenshot` | No | Capture a PNG screenshot of a guarded `tabId` as a native MCP image block. |
| `click` | Yes | Click an element selector or accessibility ref in a guarded `tabId` (subject to write-guard confirmation if it triggers a mutation). |
| `fill` | Yes | Fill an input field in a guarded `tabId`. |
| `fill_form` | Yes | Fill multiple form fields in a guarded `tabId`. |
| `press_key` | Yes | Dispatch a key press (`Enter`, `Escape`, `Tab`, …) in a guarded `tabId`. |
| `wait_for` | No | Wait for text or a selector to appear in a guarded `tabId`. |
| `evaluate` | Yes | Evaluate a read expression in a guarded `tabId`. |
| `tab_open` | Yes | Alias to open a managed tab. |
| `tab_close` | Yes | Close a managed `tabId`. |
| `status` | No | Returns plugin version, onboarded site count, CDP reachability, and SQLite store path. |

### Embedded MCP Resources

- `shop-assistant://sites` — Catalog of all onboarded sites, domains, and capabilities.
- `shop-assistant://browser` — Guidelines for interactive guarded browser tab usage.
- `shop-assistant://onboard-site` — Playbook for onboarding and testing new site adapters.

### HTTP Examples (`curl`)

```bash
# Search Irish DIY sites for an 18V drill under €150
curl -sS -X POST http://127.0.0.1:10005/tools/search -d '{"query":"einhell 18v drill","sites":["screwfix","woodies","chadwicks","mcquillantools"],"maxPrice":150}'

# Read structured product offer details
curl -sS -X POST http://127.0.0.1:10005/tools/offer -d '{"url":"https://www.screwfix.ie/p/einhell-pxc-18v-li-ion-power-x-change-cordless-combi-drill/1234x"}'

# List all onboarded sites and themes
curl -sS -X POST http://127.0.0.1:10005/tools/themes -d '{}'

# Check plugin & CDP status
curl -sS -X POST http://127.0.0.1:10005/tools/status -d '{}'
```

## Site Access Policy & Configuration

Each site's Markdown policy file in `sites/<site>.md` defines its allowed domains, URL patterns (`search`, `product`, `blocked`), and default `read: on | ask | off` and `write: ask | off` states. Override per-site settings in `$(brew --prefix)/etc/shop-assistant-mcp/config.json`:

```json
{
  "sites": {
    "screwfix": {
      "read": "on",
      "write": "off",
      "preferredStores": ["Maynooth", "Kylemore Rd"]
    }
  }
}
```

| Environment Variable | Required | Default | Purpose |
| --- | --- | --- | --- |
| `SHOP_ASSISTANT_DB` | No | `$(brew --prefix)/var/shop-assistant-mcp/shop.db` | SQLite cache and guard log path |
| `SHOP_ASSISTANT_CDP_URL` | No | `http://127.0.0.1:9222` | Brave remote debugging endpoint |
| `SHOP_ASSISTANT_CONFIG` | No | `$(brew --prefix)/etc/shop-assistant-mcp/config.json` | User config overrides |
| `SHOP_ASSISTANT_DIALOGS` | No | `on` | Set to `off` in unattended/test runs to auto-deny `ask` prompts without macOS dialogs |
| `BUGSINK_DSN` | No | Unset (disabled) | Optional error reporting DSN (`""` disables) |

Service environment variables can be placed in `$(brew --prefix)/etc/shop-assistant-mcp/env` (`chmod 600`).

## Troubleshooting

- **Client cannot connect:** confirm it supports July MCP, the service is
  running, and the client URL matches the configured port. OpenCode needs
  `"protocol": "2026-07-28"`; old `initialize` clients will fail.
- **Port occupied:** inspect the listener and stop only the service/process
  you own, or deliberately choose another port and update the client URL.
- **Works in a terminal, not as a service:** launchd does not inherit
  `~/.zshrc`. Put service variables in `$(brew --prefix)/etc/shop-assistant-mcp/env`
  (`chmod 600`) and inspect `$(brew --prefix)/var/log/shop-assistant-mcp.log` for errors.
- **Browser tools unavailable:** verify Brave's loopback CDP endpoint and dedicated profile. Handle CAPTCHA/access-denied pages manually; do not bypass them.
- **Compiler/hardening check fails:** upgrade Xcode / Command Line Tools; set
  `CXX` before first configure. CMake caches the compiler, so an intentional
  compiler change needs a fresh configuration tree.


## Development

See [docs/contributing.md](docs/contributing.md) for source builds, direct-binary
testing on port `20005`, PR/CI/release flow, and architecture details.
