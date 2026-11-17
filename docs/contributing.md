# shop-assistant-mcp: Contributing

User installation, configuration and compatibility status are in the
[README](../README.md). This reference preserves implementation details,
alternative transport schemas and validation commands. A client configuration
example is not a claim of verified July/OAuth interoperability.

Dependencies resolve directly from the pinned `slimr-down-now/*` vcpkg git
registries in `vcpkg-configuration.json` (no local overlays). CI configuration
is not evidence that a runner/platform/client has passed.

<!-- cpp-templates:begin -->
## Contents

- [Develop and test](#develop-and-test)
- [Unreleased libraries](#unreleased-libraries)
- [Rules](#rules)
- [PR, CI, and release flow](#pr-ci-and-release-flow)
- [Dependency bumps](#dependency-bumps)

## Develop and test

```bash
just doctor
just test
just lint
```

Iterate on a single doctest case with `just test "<case>"` (Debug build with ASan + UBSan, `BUGSINK_DSN=""`). For `homebrew-mcp` repos, run `just test-mcp` as well.

## Unreleased libraries

vcpkg reads extra port dirs from `VCPKG_OVERLAY_PORTS`, and an overlay wins over the registry. Point it at a dir of symlinks, one per unreleased lib; manifests never change:

```sh
mkdir -p ~/.vcpkg/overlays/dev
ln -s ~/dev/slimr/<lib>/ports/slimr-<lib> ~/.vcpkg/overlays/dev/slimr-<lib>
export VCPKG_OVERLAY_PORTS=~/.vcpkg/overlays/dev
just test
```

Remove the symlink (or `unset VCPKG_OVERLAY_PORTS`) to build against the released lib again. Keep the export out of shell startup files.

## Rules

- Never push directly to `main`; all changes land through squash-merged pull requests.
- PRs touch no release files (`VERSION`, `CHANGELOG.md`, `versions/`, `Formula/*.rb`, `Casks/*.rb`).
- Write the PR title (plus optional body) as the changelog entry; squash-merge turns it into one commit on `main`.
- Sanitize everything (no secrets, tokens, DSNs, or personal data in commits, logs, or docs).
- Run `just test` (ASan + UBSan) and `just lint` before opening or updating a PR.

## PR, CI, and release flow

- Every PR runs `test.yml`: a `gate` job skips macOS/Linux builds on docs-only PRs, and a single `ci` summary job gates merge.
- Merging to `main` triggers `release.yml` (one release at a time). It collects squash commits since the last tag (skipping docs-only ranges), picks the bump from merged PR labels (`patch` default, or `release:minor` / `release:major`; agents must ask before adding labels), writes `VERSION`/`vcpkg.json`, `CHANGELOG.md` (and `vcpkg x-add-version` for libraries), commits as the GitHub App, tags `vX.Y.Z`, and publishes the GitHub Release (plus assets and `Formula/*.rb` / `Casks/*.rb` updates).
- Pre-releases are a known option (tag/channel), not currently built.

## Dependency bumps

Bump a library baseline in `vcpkg-configuration.json` only when a bug fix, new API, or security fix requires it; otherwise wait for the next `just upgrade-deps`.
<!-- cpp-templates:end -->

## App Architecture

The standalone MCP and JSON CLI use `slimr-*` and the vcpkg-managed `lexbor` HTML parser and CSS selector engine.

```text
plugins/shop-assistant-mcp/
├── VERSION                   # Compiled into binary via CMakeLists.txt
├── CHANGELOG.md              # Release notes (## Unreleased)
├── vcpkg.json                # Independently resolved dependencies (slimr-mcp[http], lexbor)
├── vcpkg-configuration.json  # Pinned vcpkg builtin and slimr-* git registries
├── CMakeLists.txt            # App/test targets, imported dependencies, safety settings
├── justfile                  # CMake/Ninja, doctor, fmt, lint, test-mcp, and brew service lifecycle
├── README.md                 # User & operator documentation
├── AGENTS.md                 # Maintainer & AI agent conventions
├── agents/
│   └── site-reader.md        # Subagent definition shipped with the plugin (standalone tool names)
├── Formula/
│   └── shop-assistant-mcp.rb # Homebrew formula (macOS) + LaunchAgent service (with docs & sites pkgshare)
├── .github/
│   ├── actions/setup/action.yml    # Toolchain & vcpkg bootstrap + binary cache
│   └── workflows/{test,release}.yml # PR test workflow and merge-to-main release workflow
├── scripts/
│   ├── doctor.sh             # Tooling, C++26, LLVM_BIN, VCPKG_ROOT checks
│   ├── test-mcp.ts           # Bun ephemeral HTTP (`2026-07-28`) smoke check
│   ├── release.ts            # Post-merge release bump, changelog generation, tag/gh-release, formula update
│   └── formula-shared.ts     # Formula/changelog parsing and SHA-256 helpers
├── .clang-format / .clang-tidy     # Formatting and static-analysis config (llvm@23)
├── .clangd / .vscode/settings.json # Editor compilation database (build/cmake/Debug)
├── .cursor-plugin/
│   └── plugin.json           # Cursor plugin manifest -> http://127.0.0.1:10005/mcp
├── .claude-plugin/
│   ├── plugin.json           # Claude plugin manifest -> http://127.0.0.1:10005/mcp
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
│   │   ├── cdp.hpp           # Chrome DevTools Protocol client over `cpp-httplib`
│   │   ├── url.hpp           # RFC 3986 URL parser and relative resolver
│   │   ├── adapter.hpp       # `Offer`, `ProductPage`, `Review`, `SiteAdapter`, price/stock parsers, `AdapterRegistry`
│   │   └── pages.hpp         # Page settlement & anti-bot challenge detection (`isChallenge`)
│   ├── sites/
│   │   ├── all_sites.hpp     # Registers all 22 site adapters into `AdapterRegistry`
│   │   └── <site>.hpp        # 22 site-specific HTML/JSON-LD parsers (`screwfix.hpp`, `amazon.hpp`, …)
│   ├── app.hpp               # `shop::make_shop_assistant_app()` registering all 19 tools, resources & optional reporting
│   └── main.cpp              # Standalone CLI and Streamable HTTP (`127.0.0.1:10005`) entrypoint
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
- **`cpp-httplib` (`<httplib.h>`):** CDP WebSocket connection and HTTP target discovery against Brave (`127.0.0.1:9222`).
- **`<sqlite.hpp>` (`-lsqlite3`):** Page cache, site health tracking, and guard audit logging in `$(brew --prefix)/var/shop-assistant-mcp/shop.db`.
- **`<yaml.hpp>` (`rapidyaml`):** Parsing `sites/<site>.md` YAML policy frontmatter (`parse_frontmatter`).
- **`<json.hpp>` (`glaze`):** JSON-LD (`@type: Product`) and CDP message parsing.
- **`<bugsink.hpp>` (`-lcurl`):** Optional error reporting, enabled only by `BUGSINK_DSN` and tagged with `shop-assistant-mcp@<VERSION>`.
- **`<test.hpp>` (`doctest`):** Offline HTML/JSON-LD fixture test suite compiled with `-fsanitize=address,undefined`.

## HTTP & Client Configuration (MCP `2026-07-28`)

`shop-assistant-mcp` defaults to Streamable HTTP on `127.0.0.1:10005/mcp` (`shop-assistant-mcp http`; pass `--host 0.0.0.0` to expose) while preserving direct JSON CLI invocation (`shop-assistant-mcp <tool> ['{...}']`).

> **Breaking change (MCP `2026-07-28`):** Legacy `initialize` / `notifications/initialized` session handshakes are replaced by stateless `server/discover` with `MCP-Protocol-Version: 2026-07-28` and `Mcp-Method` headers (legacy `initialize` returns `-32601`).

### Direct-Binary Dev Loop (`http://127.0.0.1:20005/mcp`)

```bash
just build
./build/bin/shop-assistant-mcp http --port 20005
```

### Homebrew Service (`http://127.0.0.1:10005/mcp`)

```bash
just install         # taps GitHub repo, installs released formula, runs brew test, starts service
just daemon-start    # starts LaunchAgent service and verifies server/discover
just daemon-status   # checks brew services info + HTTP server/discover
just daemon-stop     # stops LaunchAgent service
just daemon-logs     # tails $(brew --prefix)/var/log/shop-assistant-mcp.log
```

### Client Config Spellings

**Claude Code / `.mcp.json` (HTTP — requires `"type": "http"`):**

```json
{
  "mcpServers": {
    "shop-assistant-mcp": {
      "type": "http",
      "url": "http://127.0.0.1:10005/mcp"
    }
  }
}
```

**Cursor (`.cursor/mcp.json` or `.cursor-plugin/plugin.json`):**

```json
{
  "mcpServers": {
    "shop-assistant-mcp": {
      "url": "http://127.0.0.1:10005/mcp"
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
      "url": "http://127.0.0.1:10005/mcp",
        "protocol": "2026-07-28"
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
- **SQLite Page Cache & Guard Log (`src/core/db.hpp`):** Caches fetched pages and records site health checks and guard decisions in `$(brew --prefix)/var/shop-assistant-mcp/shop.db`.

## Building & Testing

Fixtures are original, minimal synthetic markup, not redistributed retailer page captures.
The current tests exercise seven search parsers and registration of all 22 adapters;
they do not establish full live-site compatibility. Unused captured HTML/JSON, reviews,
and matching datasets are intentionally not distributed. The ISC license covers the
original code and synthetic fixtures, not third-party site content or dependencies.

Build on macOS with `llvm@23` (`clang++` with `-stdlib=libc++` and static `libc++.a` + `libc++abi.a`, `LLVM_BIN`), C++26, libc++ FAST hardening, CMake 3.31+, Ninja, [just](https://just.systems), [vcpkg](https://vcpkg.io), and Bun. Set `VCPKG_ROOT` to a bootstrapped checkout and put `vcpkg` on `PATH`.

`just configure`, `test`, `build`, and `lint` automatically run `deps`, installing manifest dependencies under `build/deps/`. CMake uses installed `slimr::mcp_http`, `slimr::sqlite`, `slimr::yaml`, and `lexbor::lexbor_static` targets, preserving static SQLite/FTS5 and Lexbor linkage; manifest auto-install is disabled.

Debug sanitizers and Release IPO builds have separate `build/cmake/Debug/` and `build/cmake/Release/` graphs. Both enable libc++ hardening and zero automatic-variable initialization; Debug adds ASan and fatal UBSan. Set `CXX` before the first configure to override the default `llvm@23` compiler; subsequent configures preserve CMake's cached compiler. CTest runs offline fixtures from the repository root with `SHOP_ASSISTANT_DIALOGS=off`, a finite timeout, and isolated repo-local HOME/XDG/temporary directories.

```bash
just doctor   # Verify required tools, C++26 compiler, LLVM_BIN, and VCPKG_ROOT
just test     # Run core, site-parser, CLI, HTTP, and thread-safety tests with ASan + UBSan
just build    # Build Release binary at build/bin/shop-assistant-mcp
just lint     # Check formatting and analyze src/main.cpp using compile_commands.json
just test-mcp # Verify ephemeral HTTP (http --port 0) against MCP 2026-07-28
```

`just test` and `just test-mcp` run the server with an isolated HOME/XDG environment, no external CLI PATH or credentials, `SHOP_ASSISTANT_DIALOGS=off`, and Bugsink disabled; they never open browser tabs or invoke external retail sites. `platforms` is set to `macos` (`macos-26` runner in CI).
