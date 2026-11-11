# shop-assistant-mcp: Developer Reference

User installation, configuration and compatibility status are in the
[README](../README.md). This reference preserves implementation details,
alternative transport schemas and validation commands. A client configuration
example is not a claim of verified July/OAuth interoperability.

Current HTTP source builds use local slimr and libhv overlays. The pinned
published registry baseline predates these HTTP changes; do not claim that
baseline builds the current HTTP app without overrides. CI configuration is
not evidence that a runner/platform/client has passed.

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

## HTTP & Client Configuration (MCP `2026-07-28`)

`shop-assistant-mcp` defaults to HTTP-first Streamable HTTP on `http://127.0.0.1:8085/mcp` (`shop-assistant-mcp http`) while preserving stdio (`shop-assistant-mcp mcp`) and direct JSON CLI invocation (`shop-assistant-mcp <tool> ['{...}']`).

> **Breaking change (MCP `2026-07-28`):** Legacy `initialize` / `notifications/initialized` session handshakes are replaced by stateless `server/discover` with `MCP-Protocol-Version: 2026-07-28` and `Mcp-Method` headers (legacy `initialize` returns `-32601`). Non-loopback bindings require `--allow-non-loopback`, and `--port 0` requires `--allow-ephemeral-port`.

### Homebrew Service (`http://127.0.0.1:8085/mcp`)

```bash
just install-local   # Local source-build installation; the release formula is not finalized
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
    "servers": {
      "shop-assistant-mcp": {
      "type": "remote",
      "url": "http://127.0.0.1:8085/mcp",
        "protocol": "2026-07-28"
      }
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

`just configure`, `test`, `build`, and `lint` automatically run `deps`, installing manifest dependencies under `build/deps/`. CMake uses installed `slimr::mcp_http`, `slimr::sqlite`, `slimr::yaml`, `slimr::ws`, and `lexbor::lexbor_static` targets, preserving static SQLite/FTS5 and Lexbor linkage; manifest auto-install is disabled. Local overlay ports must already be registered; no source-checkout include overrides or global executable installs are used. Registry publication is separate.

Debug sanitizers and Release IPO builds have separate `build/cmake/Debug/` and `build/cmake/Release/` graphs. Both enable libc++ hardening and zero automatic-variable initialization; Debug adds ASan and fatal UBSan. Set `CXX` before the first configure to override the default Apple compiler on macOS or `clang++` on Linux; subsequent configures preserve CMake's cached compiler even when clang-tidy is added to `PATH`. CTest runs offline fixtures from the repository root with `SHOP_ASSISTANT_DIALOGS=off`, a finite timeout, and isolated repo-local HOME/XDG/temporary directories. Release builds run those tests too.

```bash
just test     # Run core, site-parser, CLI, HTTP, and thread-safety tests with ASan + UBSan
just build    # Build/test Release binary at build/bin/shop-assistant-mcp
just lint     # Analyze app and test translation units using compile_commands.json (supports CLANG_TIDY)
just test-mcp # Verify stdio (mcp) and ephemeral HTTP (http --port 0 --allow-ephemeral-port) against MCP 2026-07-28
just run --help # Run the existing Release binary without rebuilding
```

`just test` and `just test-mcp` run the server with an isolated HOME/XDG environment, no external CLI PATH or credentials, `SHOP_ASSISTANT_DIALOGS=off`, and Bugsink disabled; they never open browser tabs or invoke external retail sites. Linux/macOS PR CI is configured for the same commands and requires the tested full `VCPKG_REF` commit variable. Current July HTTP builds require matching local slimr/libhv overlays until a corresponding registry release is published. Linux execution and remote merge-gate enforcement are not claimed by local macOS verification.
