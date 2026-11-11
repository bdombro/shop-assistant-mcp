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

## Features

- Search and compare offers, product specs, reviews, delivery, and stock across onboarded stores.
- Read product pages and use guarded browser tabs for pages that need interaction.
- Checkout, payment, login, and account pages are blocked; browser writes require confirmation.
- Small footprint: a standalone MCP binary reuses your existing Brave browser rather than bundling one.

## `.mcp.json` Configuration

In the `assistant2` workspace, `shop-assistant-mcp` is mounted into the unified `build/bin/assistant-mcp` gateway under the `shop_` prefix (`shop_search`, `shop_offer`, `shop_reviews`, `shop_page`, `shop_themes`, `shop_open`, `shop_navigate`, `shop_click`, `shop_fill`, `shop_fill_form`, `shop_press_key`, `shop_wait_for`, `shop_evaluate`, `shop_snapshot`, `shop_screen`, `shop_screenshot`, `shop_tab_open`, `shop_tab_close`, `shop_status`).

To run `shop-assistant-mcp` as a standalone MCP server in `.mcp.json`:

```json
{
  "mcpServers": {
    "shop-assistant-mcp": {
      "command": "/absolute/path/to/shop-assistant-mcp/build/bin/shop-assistant-mcp",
      "args": ["mcp"],
      "env": {
        "SHOP_ASSISTANT_CDP_URL": "http://127.0.0.1:9222"
      }
    }
  }
}
```

## App Architecture

The standalone MCP and JSON CLI use `slimr-cpp` and the vcpkg-managed `lexbor` HTML parser and CSS selector engine.

```text
plugins/shop-assistant-mcp/
├── vcpkg.json                # Package dependencies installed under build/deps/
├── CMakeLists.txt            # App/test targets, imported dependencies, safety settings
├── justfile                  # CMake/Ninja configure, test, build, lint, test-mcp
├── README.md                 # User & operator documentation
├── AGENTS.md                 # Maintainer & AI agent conventions
├── agents/
│   └── site-reader.md        # Subagent definition copied to `.claude/agents/` by `just mcp-configs`
├── docs/                     # Embedded MCP resources (`agent-guide.md`, `categories/`)
├── sites/                    # 25 site policy Markdown files (`<site>.md` with YAML frontmatter)
├── fixtures/                 # Synthetic HTML & JSON-LD cleaner fixtures
├── src/
│   ├── core/
│   │   ├── dom.hpp           # RAII C++26 wrapper around `lexbor` (`Document`, `Node`, `querySelectorAll`, `findLd`)
│   │   ├── markdown.hpp      # HTML sanitizer & Markdown renderer (`cleanHtml`, `pageView`, `splitSections`)
│   │   ├── policy.hpp        # URL/mutation guard policy (`parseSiteDoc` via `yaml.hpp`, `decide`)
│   │   ├── db.hpp            # SQLite `Store` (`shop.db`: page cache, guard event log, site health checks)
│   │   ├── cdp.hpp           # Chrome DevTools Protocol client over `ws.hpp` & `curl.hpp`
│   │   ├── adapter.hpp       # `Offer`, `ProductPage`, `Review`, `SiteAdapter`, price/stock parsers, `AdapterRegistry`
│   │   └── pages.hpp         # Page settlement & anti-bot challenge detection (`isChallenge`)
│   ├── sites/
│   │   ├── all_sites.hpp     # Registers all 22 site adapters into `AdapterRegistry`
│   │   └── <site>.hpp        # 22 site-specific HTML/JSON-LD parsers (`screwfix.hpp`, `amazon.hpp`, …)
│   ├── app.hpp               # `shop::make_shop_assistant_app()` registering all 19 tools, resources & optional reporting
│   └── main.cpp              # Standalone CLI & stdio MCP entrypoint
├── tests/
│   ├── test_core.hpp         # 27 core DOM, Markdown, policy, and SQLite test cases
│   ├── test_sites.hpp        # Site adapter fixture test suites
│   └── test_main.cpp         # Combined test runner
└── build/bin/
    └── shop-assistant-mcp    # Standalone native Release binary
```

### Libraries Used

- **`lexbor`:** vcpkg-managed static C HTML5 parser and CSS selector engine wrapped in `src/core/dom.hpp`.
- **`<mcp.hpp>` & `<reflect.hpp>`:** Typed MCP tool definitions, compile-time JSON Schema generation, native MCP image blocks (`shop_screenshot`), and narrow JSON CLI.
- **`<ws.hpp>` & `<curl.hpp>` (`-lcurl`):** CDP WebSocket connection and HTTP target discovery against Brave (`127.0.0.1:9222`) + Shopify UCP catalog fetches.
- **`<sqlite.hpp>` (`-lsqlite3`):** Page cache, site health tracking, and guard audit logging in `~/.local/state/shop-assistant/shop.db`.
- **`<yaml.hpp>` (`rapidyaml`):** Parsing `sites/<site>.md` YAML policy frontmatter (`parse_frontmatter`).
- **`<json.hpp>` (`glaze`):** JSON-LD (`@type: Product`) and CDP message parsing.
- **`<bugsink.hpp>` (`-lcurl`):** Optional error reporting, enabled only by `BUGSINK_DSN`.
- **`<test.hpp>` (`doctest`):** Offline HTML/JSON-LD fixture test suite compiled with `-fsanitize=address,undefined`.

## Key Capabilities & Architecture

- **22 Onboarded Site Adapters (`src/sites/`):**
  - **Home improvement & DIY:** `screwfix`, `diy` (B&Q Ireland), `woodies`, `chadwicks`, `brooks`, `tjomahony`, `mcmahons`, `mcquillantools`, `tooldepotireland`, `mydiy`, `toolsidee`, `strahan` (UCP)
  - **Electronics & Appliances:** `currys`, `powercity`, `harveynorman` (UCP), `did` (UCP)
  - **General retail & Marketplaces:** `amazon` (`.ie` / `.co.uk` / `.de` / `.com`), `very`, `houseoffraser`, `ebay`, `adverts`, `donedeal`, `huntoffice`
  - **Shopping aggregators:** `bing` (Bing Shop), `duckduckgo`
- **Fast `lexbor` DOM + JSON-LD extraction (`src/core/dom.hpp`):** Parses full retail HTML pages and runs complex CSS selectors and `@context: schema.org` JSON-LD extraction in <0.5 ms with shared RAII ownership of the HTML tree and CSS selector arena.
- **Prompt-injection-hardened Markdown view (`src/core/markdown.hpp`):** Strips scripts, styles, cookie banners, hidden DOM subtrees, and invisible/bidi Unicode characters before converting arbitrary product pages into compact Markdown sections (`pageView`, `splitSections`).
- **Guarded CDP Browser Tabs (`src/core/cdp.hpp` & `src/core/policy.hpp`):**
  - Connects to Brave (`--remote-debugging-port=9222`) over WebSockets and only touches tabs opened by `shop-assistant` (never inspecting your personal tabs).
  - Intercepts requests via CDP `Fetch`: checkout, payment, login, and account URLs (`PageKind::Blocked`) are unconditionally blocked.
  - Every mutating browser action (`click`, `fill`, `fill-form`, `press-key` triggering a write) requires explicit confirmation in a native macOS dialog (`write: ask`), and background page beacons/prefetches are blocked silently without prompting.
- **SQLite Page Cache & Guard Log (`src/core/db.hpp`):** Caches fetched pages and records site health checks and guard decisions in `~/.local/state/shop-assistant/shop.db`.

## Tools & CLI Commands

Every tool is exposed over MCP (`shop-assistant-mcp mcp` or via `assistant-mcp` under the `shop_` prefix) and on the CLI (`shop-assistant-mcp <tool> ['{"arg":"val"}']`). Existing `shop-assistant://` resources and data paths are unchanged.

| Tool | Gateway Name | Mutating | Description |
| --- | --- | --- | --- |
| `search` | `shop_search` | No | Search one or more onboarded sites (or a category theme like `"diy"`, `"electronics"`) concurrently, returning normalized offers (`title`, `price`, `currency`, `url`, `stock`, `delivery`, `rating`, `specs`) filtered by `minPrice`, `maxPrice`, `inStockOnly`, and token budget. |
| `offer` | `shop_offer` | No | Fetch structured product details (`ProductPage`: price, stock, specs table, description, images, delivery options) from a product URL using its site adapter or JSON-LD. |
| `reviews` | `shop_reviews` | No | Extract customer review summaries and individual reviews (`rating`, `title`, `body`, `date`, `verified`) for a product URL. |
| `page` | `shop_page` | No | Read any allowed URL on an onboarded site as clean, sectioned Markdown (`section`, `maxTokens`), served from `shop.db` cache when fresh. |
| `themes` | `shop_themes` | No | List onboarded sites grouped by category theme, along with their current `read`/`write` policy and health status. |
| `open` | `shop_open` | Yes | Open a new guarded browser tab on an onboarded site URL and return its `tabId` and accessibility/Markdown snapshot. |
| `navigate` | `shop_navigate` | Yes | Navigate an existing guarded `tabId` to a new allowed URL. |
| `snapshot` | `shop_snapshot` | No | Capture the current accessibility tree / interactive element refs of a guarded `tabId`. |
| `screen` | `shop_screen` | No | Read the current rendered text/Markdown content of a guarded `tabId`. |
| `screenshot` | `shop_screenshot` | No | Capture a PNG screenshot of a guarded `tabId` as a native MCP image block. |
| `click` | `shop_click` | Yes | Click an element selector or accessibility ref in a guarded `tabId` (subject to write-guard confirmation if it triggers a mutation). |
| `fill` | `shop_fill` | Yes | Fill an input field in a guarded `tabId`. |
| `fill-form` / `fill_form` | `shop_fill_form` | Yes | Fill multiple form fields in a guarded `tabId`. |
| `press-key` / `press_key` | `shop_press_key` | Yes | Dispatch a key press (`Enter`, `Escape`, `Tab`, …) in a guarded `tabId`. |
| `wait-for` / `wait_for` | `shop_wait_for` | No | Wait for text or a selector to appear in a guarded `tabId`. |
| `evaluate` | `shop_evaluate` | Yes | Evaluate a read expression in a guarded `tabId`. |
| `tab-open` / `tab_open` | `shop_tab_open` | Yes | Alias to open a managed tab. |
| `tab-close` / `tab_close` | `shop_tab_close` | Yes | Close a managed `tabId`. |
| `status` | `shop_status` | No | Returns plugin version, onboarded site count, CDP reachability, and SQLite store path. |

### Embedded MCP Resources

- `shop-assistant://sites` — Catalog of all onboarded sites, domains, and capabilities.
- `shop-assistant://browser` — Guidelines for interactive guarded browser tab usage.
- `shop-assistant://onboard-site` — Playbook for onboarding and testing new site adapters.

### CLI Examples

```bash
# Search Irish DIY sites for an 18V drill under €150
./build/bin/shop-assistant-mcp search '{"query":"einhell 18v drill","sites":["screwfix","woodies","chadwicks","mcquillantools"],"maxPrice":150}'

# Read structured product offer details
./build/bin/shop-assistant-mcp offer '{"url":"https://www.screwfix.ie/p/einhell-pxc-18v-li-ion-power-x-change-cordless-combi-drill/1234x"}'

# List all onboarded sites and themes
./build/bin/shop-assistant-mcp themes

# Check plugin & CDP status
./build/bin/shop-assistant-mcp status
```

## Site Access Policy & Configuration

Each site's Markdown policy file in `sites/<site>.md` defines its allowed domains, URL patterns (`search`, `product`, `blocked`), and default `read: on | ask | off` and `write: ask | off` states. Override per-site settings in `~/.config/shop-assistant/config.json`:

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

| Environment Variable | Default | Purpose |
| --- | --- | --- |
| `SHOP_ASSISTANT_DB` | `~/.local/state/shop-assistant/shop.db` | SQLite cache and guard log path |
| `SHOP_ASSISTANT_CDP_URL` | `http://127.0.0.1:9222` | Brave remote debugging endpoint |
| `SHOP_ASSISTANT_CONFIG` | `~/.config/shop-assistant/config.json` | User config overrides |
| `SHOP_ASSISTANT_DIALOGS` | `on` | Set to `off` in unattended/test runs to auto-deny `ask` prompts without macOS dialogs |
| `BUGSINK_DSN` | Unset (disabled) | Optional error reporting DSN (`""` disables) |

## Building & Testing

Fixtures are original, minimal synthetic markup, not redistributed retailer page captures.
The current tests exercise seven search parsers and registration of all 22 adapters;
they do not establish full live-site compatibility. Unused captured HTML/JSON, reviews,
and matching datasets are intentionally not distributed. The ISC license covers the
original code and synthetic fixtures, not third-party site content or dependencies.

Build with Clang/C++26 and libc++ FAST hardening, CMake 3.31+, Ninja, [just](https://just.systems), [vcpkg](https://vcpkg.io), and a compatible clang-tidy. Set `VCPKG_ROOT` to a bootstrapped checkout and put `vcpkg` on `PATH`; configure the local overlay explicitly:

```bash
export VCPKG_OVERLAY_PORTS="$VCPKG_ROOT/local-ports"
```

`just configure`, `test`, `build`, and `lint` automatically run `deps`, installing manifest dependencies under `build/deps/`. CMake uses installed `slimr::mcp`, `slimr::sqlite`, `slimr::yaml`, `slimr::ws`, and `lexbor::lexbor_static` targets, preserving static SQLite/FTS5 and Lexbor linkage; manifest auto-install is disabled. Local overlay ports must already be registered; no source-checkout include overrides or global executable installs are used. Registry publication is separate.

Debug sanitizers and Release IPO builds have separate `build/cmake/Debug/` and `build/cmake/Release/` graphs. Both enable libc++ hardening and zero automatic-variable initialization; Debug adds ASan and fatal UBSan. Set `CXX` before the first configure to override the default Apple compiler on macOS or `clang++` on Linux; subsequent configures preserve CMake's cached compiler even when clang-tidy is added to `PATH`. CTest runs offline fixtures from the repository root with `SHOP_ASSISTANT_DIALOGS=off`, a finite timeout, and isolated repo-local HOME/XDG/temporary directories. Release builds run those tests too.

```bash
just test     # Run core tests, seven synthetic search-parser cases and the 22-adapter registry check with ASan + UBSan
just build    # Build/test Release binary at build/bin/shop-assistant-mcp
just lint     # Analyze app and test translation units using compile_commands.json
just test-mcp # Inspector 2.14.0 initialize + tools/list only (requires compatible Node/npm)
just run --help # Run the existing Release binary without rebuilding
```

Inspector runs the server with an isolated HOME/XDG environment, no external CLI PATH or credentials, and Bugsink disabled; it never opens browser tabs or invokes data tools. Linux/macOS PR CI uses the same commands and requires the tested full `VCPKG_REF` commit variable; slimr packages use the pinned public registry without a dependency token. Linux execution and remote merge-gate enforcement are not claimed by local macOS verification.
