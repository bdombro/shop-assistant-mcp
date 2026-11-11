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

## HTTP & Client Configuration (MCP `2026-07-28`)

`shop-assistant-mcp` defaults to HTTP-first Streamable HTTP on `http://127.0.0.1:8085/mcp` (`shop-assistant-mcp http`) while preserving stdio (`shop-assistant-mcp mcp`) and direct JSON CLI invocation (`shop-assistant-mcp <tool> ['{...}']`).

> **Breaking change (MCP `2026-07-28`):** Legacy `initialize` / `notifications/initialized` session handshakes are replaced by stateless `server/discover` with `MCP-Protocol-Version: 2026-07-28` and `Mcp-Method` headers (legacy `initialize` returns `-32601`). Non-loopback bindings require `--allow-non-loopback`, and `--port 0` requires `--allow-ephemeral-port`.

### Homebrew Service (`http://127.0.0.1:8085/mcp`)

```bash
just install-local   # or: brew tap bdombro/shop-assistant-mcp && brew install bdombro/shop-assistant-mcp/shop-assistant-mcp
just start           # starts LaunchAgent service on 127.0.0.1:8085 and verifies server/discover
just status          # checks brew services info + HTTP server/discover
just stop            # stops LaunchAgent service
```

### Client Config Spellings

**Claude Code / `.mcp.json` (HTTP — requires `"type": "http"`):**

```json
{
  "mcpServers": {
    "shop-assistant-mcp": {
      "type": "http",
      "url": "http://127.0.0.1:8085/mcp"
    }
  }
}
```

**Cursor (`.cursor/mcp.json` or `.cursor-plugin/plugin.json`):**

```json
{
  "mcpServers": {
    "shop-assistant-mcp": {
      "url": "http://127.0.0.1:8085/mcp"
    }
  }
}
```

**OpenCode (`opencode.jsonc` — uses `"type": "remote"`):**

```json
{
  "mcp": {
    "shop-assistant-mcp": {
      "type": "remote",
      "url": "http://127.0.0.1:8085/mcp"
    }
  }
}
```

**Standalone stdio (`.mcp.json` fallback):**

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
│   └── site-reader.md        # Subagent definition shipped with the plugin (standalone tool names)
├── Formula/
│   └── shop-assistant-mcp.rb # Homebrew formula + LaunchAgent service (with docs & sites pkgshare)
├── scripts/
│   ├── homebrew.mjs          # Local formula staging & brew services lifecycle
│   └── test-mcp.mjs          # Stdio + ephemeral HTTP (`2026-07-28`) smoke check
├── .cursor-plugin/
│   └── plugin.json           # Cursor plugin manifest -> http://127.0.0.1:8085/mcp
├── .claude-plugin/
│   ├── plugin.json           # Claude plugin manifest -> http://127.0.0.1:8085/mcp
│   └── marketplace.json      # Claude plugin marketplace descriptor
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
│   └── main.cpp              # Standalone CLI, stdio MCP, and Streamable HTTP (`127.0.0.1:8085`) entrypoint
├── tests/
│   ├── test_core.hpp         # Core DOM, Markdown, policy, SQLite, HTTP, CLI, and thread-safety test cases
│   ├── test_sites.hpp        # Site adapter fixture test suites
│   └── test_main.cpp         # Combined test runner
└── build/bin/
    └── shop-assistant-mcp    # Standalone native Release binary
```

### Libraries Used

- **`lexbor`:** vcpkg-managed static C HTML5 parser and CSS selector engine wrapped in `src/core/dom.hpp`.
- **`<mcp.hpp>`, `<mcp_http.hpp>` & `<reflect.hpp>`:** Typed MCP tool definitions, compile-time JSON Schema generation, native MCP image blocks (`screenshot`), HTTP (`2026-07-28`), and narrow JSON CLI.
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

Every tool is exposed over MCP (`shop-assistant-mcp http` on `http://127.0.0.1:8085/mcp`, `shop-assistant-mcp mcp` over stdio) and on the CLI (`shop-assistant-mcp <tool> ['{"arg":"val"}']`). Existing `shop-assistant://` resources and data paths are unchanged.

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

## macOS Build Prerequisites (vcpkg setup)

Install [Homebrew](https://brew.sh) and current Xcode Command Line Tools
(`xcode-select --install`; finish installation before continuing), or full Xcode.
Apple Clang 21 is the verified C++26/libc++ toolchain; older installations need
upgrading. These instructions assume Apple Silicon Homebrew at `/opt/homebrew`.

Add this environment snippet to `~/.zshrc`, then open a new terminal (or run
`source ~/.zshrc`). Use `~/.zshrc`, not `~/.zshenv`, to avoid changing
non-interactive zsh scripts:

`local-ports` is the dedicated vcpkg overlay directory for local development
linking: `just publish --local --all` registers symlinks to your slimr-cpp checkout,
overriding published packages without making a registry release. See the
[publishing guide on slimr-cpp](https://github.com/bdombro/slimr-cpp/blob/main/docs/publishing.md#local-environment).

```bash
export VCPKG_ROOT="$HOME/.local/share/vcpkg"
export VCPKG_OVERLAY_PORTS="$VCPKG_ROOT/local-ports"
# Use your existing workspace checkout instead when available.
export SLIMR_CPP="${SLIMR_CPP:-$HOME/dev/bdombro/slimr-cpp}"
export CXX="/usr/bin/clang++"
export CLANG_TIDY="/opt/homebrew/opt/llvm/bin/clang-tidy"
export PATH="$VCPKG_ROOT:/opt/homebrew/opt/ruby/bin:/opt/homebrew/bin:$PATH"
```

Then install and build from the repository root:

```bash
brew install cmake ninja just llvm node ruby autoconf automake libtool pkgconf
mkdir -p "$(dirname "$VCPKG_ROOT")"
if [ ! -d "$VCPKG_ROOT/.git" ]; then
  git clone https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT"
fi
"$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
mkdir -p "$(dirname "$SLIMR_CPP")"
if [ ! -d "$SLIMR_CPP/.git" ]; then
  git clone https://github.com/bdombro/slimr-cpp.git "$SLIMR_CPP"
fi
(cd "$SLIMR_CPP" && just publish --local --all)
ln -s "$SLIMR_CPP/ports/libhv" "$VCPKG_OVERLAY_PORTS/libhv"

"$CXX" --version
vcpkg version
BUGSINK_DSN="" just test
just build
```

The libhv link is a one-time step: if it exists, verify that it points to the
chosen checkout's `ports/libhv` before skipping the command. Never overwrite
a conflicting port directory or modify installed headers/caches. Current HTTP
builds need both local slimr overlays and the separately pinned libhv port;
the older published registry baseline alone is not an HTTP build substitute.
`just deps` installs dependencies into `build/deps/`; vcpkg supplies Lexbor,
static SQLite/FTS5 and WebSocket-enabled curl. Separate Homebrew library
installs are unnecessary.

Bootstrapping puts the executable at `$VCPKG_ROOT/vcpkg`, with no `bin/`
subdirectory. Keg-only Homebrew Ruby supplies `/opt/homebrew/opt/ruby/bin/ruby`.
Homebrew LLVM supplies lint tooling; select a different compatible `CXX`
before first configure if needed, since CMake caches it. For reproducible
vcpkg tooling, check out the tested full `VCPKG_REF` commit before bootstrapping;
package registry baselines pin resolution separately.
For live shopping, separately install Brave with
`brew install --cask brave-browser` and configure its loopback CDP endpoint
as described above. macOS supplies `osascript` for confirmation dialogs.
Build/test commands do not launch a browser, open shopping tabs, or start
services.

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
just test     # Run core, site-parser, CLI, HTTP, and thread-safety tests with ASan + UBSan
just build    # Build/test Release binary at build/bin/shop-assistant-mcp
just lint     # Analyze app and test translation units using compile_commands.json (supports CLANG_TIDY)
just test-mcp # Verify stdio (mcp) and ephemeral HTTP (http --port 0 --allow-ephemeral-port) against MCP 2026-07-28
just run --help # Run the existing Release binary without rebuilding
```

`just test` and `just test-mcp` run the server with an isolated HOME/XDG environment, no external CLI PATH or credentials, `SHOP_ASSISTANT_DIALOGS=off`, and Bugsink disabled; they never open browser tabs or invoke external retail sites. Linux/macOS PR CI uses the same commands and requires the tested full `VCPKG_REF` commit variable; slimr packages use the pinned public registry without a dependency token. Linux execution and remote merge-gate enforcement are not claimed by local macOS verification.
