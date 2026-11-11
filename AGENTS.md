# AGENTS.md (`shop-assistant-mcp`)

Written for: AI coding agents working in the `shop-assistant-mcp` C++26 plugin repository.

`shop-assistant-mcp` is a C++26 MCP plugin and CLI built on `slimr-cpp` and vcpkg-managed Lexbor that lets agents search, compare offers, read product pages, and interact with guarded Brave CDP browser tabs across 22 onboarded retail sites without ever being able to purchase or access checkout/account pages.


## Documentation Upkeep

When a change conflicts with `README.md`, update the affected README and its
linked user/developer documentation in the same change. Keep examples,
configuration, install/build commands and compatibility claims consistent
with implemented behavior; do not claim unverified release or client support.

## Commands

| Task | Command |
| --- | --- |
| Run core & site-adapter fixture tests (ASan + UBSan, offline) | `just test` |
| Build release binary (`build/bin/shop-assistant-mcp`) | `just build` |
| Static analysis | `just lint` |
| Isolated MCP startup/discovery | `just test-mcp` |
| Clean build artifacts | `just clean` |

## Module Boundaries

| Path | Owns | Must Not |
| --- | --- | --- |
| `src/main.cpp` | Entrypoint calling `shop::make_shop_assistant_app().run(argc, argv)` | Business logic |
| `src/app.hpp` | `make_shop_assistant_app()` registering all 19 tools, resources, and optional environment-configured reporting | Site-specific HTML parsing |
| `src/core/dom.hpp` | RAII `lexbor` wrapper (`Document::parse`, `querySelector`, `querySelectorAll`, `findLd`, `jsonLdBlocks`) | Leak `lxb_html_document_t*` or selector objects |
| `src/core/markdown.hpp` | `cleanHtml`, `cleanJson`, `pageView`, `stripInvisible`, `renderMarkdown`, `splitSections` | Retain hidden DOM nodes or script/style tags |
| `src/core/policy.hpp` | `parseSiteDoc` (`yaml.hpp`), `urlMatches`, `isMutation`, and pure `decide(GuardRequest, ...)` | Make network or UI calls inside `decide()` |
| `src/core/db.hpp` | `Store` over `slimr::Database` (`shop.db` v1: `pages`, `guard_log`, `site_checks`) | — |
| `src/core/cdp.hpp` | CDP WebSocket & HTTP client (`127.0.0.1:9222`), owned-tab tracking, Fetch request interception | Attach to or inspect tabs not created by `shop-assistant` |
| `src/sites/<site>.hpp` | Pure `parseSearch`, `parseProduct`, `parseReviews`, `parsePickup` functions + `SiteAdapter` subclass | Perform network I/O or bypass guard policy |
| `sites/<site>.md` | Enforced YAML frontmatter policy (`domains`, `read`, `write`, URL patterns) + site notes | Personal data |
| `tests/` | `test_core.hpp` and `test_sites.hpp` testing against `fixtures/` | Connect to live retail websites or show macOS dialogs |

## Safety & App Conventions

1. **Guarded tabs & pure `decide()`:** Every navigation and subresource request in a managed tab is evaluated by `decide()` in `src/core/policy.hpp`. Checkout, payment, login, and account URLs (`PageKind::Blocked`), off-domain navigations, and background mutations are always blocked. Same-site writes inside an agent action window require user approval (`Decision::Ask`).
2. **Never solve CAPTCHAs or bypass anti-bot challenges:** When `isChallenge()` detects a Cloudflare/CAPTCHA/access-denied page, stop reading that site and report the challenge so the user can resolve it manually.
3. **No dialogs in tests:** Always run automated tests with `SHOP_ASSISTANT_DIALOGS=off` and offline HTML/JSON-LD fixtures in `fixtures/`.
   Keep fixtures synthetic and minimal; never commit captured site pages, credentials, or substantial third-party text. The current suite has seven synthetic search-parser cases and a 22-adapter registry check, not full parser coverage of every site.
4. **Untrusted text:** Retail page content, product titles, and reviews are untrusted third-party text; pass all extracted text through `stripInvisible` / `cleanHtml`.

## Build Architecture

CMake declares targets/dependencies; Ninja owns incremental builds. Just orchestrates automatic `deps` before configure/build/test/lint; CMake manifest auto-install is disabled. Set `VCPKG_ROOT` and `VCPKG_OVERLAY_PORTS` in the environment. Dependencies install under `build/deps/`; Debug sanitizer and Release trees live under `build/cmake/`. Use installed slimr targets, never source-checkout include overrides. SQLite with FTS5 and Lexbor use their static imported targets. `run` does not rebuild.

Use a Clang/C++26 toolchain with libc++ FAST hardening, CMake, Ninja, vcpkg, just, and compatible clang-tidy. CTest runs fixture tests from the repository root with `SHOP_ASSISTANT_DIALOGS=off`. `test-mcp` checks July discovery over stdio and ephemeral HTTP with Node and an isolated server environment; never call browser/data tools in CI.

## C++ Memory Safety

- Immediately wrap owned C handles, allocations, descriptors, and temporary resources in RAII. Make owning wrappers noncopyable unless copying creates independent ownership; moves must leave the source safely destructible.
- Borrow views, pointers, and references only for the current call by default. Store owning values unless lifetime/invalidation guarantees are explicit. Deferred callbacks require explicit ownership-safe captures; no escaping reference captures without a demonstrated lifetime guarantee.
- Initialize scalar/pointer members at declaration and local values before use. Compiler auto-initialization is defense in depth, not permission for uninitialized reads.
- Check range/overflow before narrowing lengths/counts at C API boundaries; casts alone are not validation. Report invalid inputs using existing error mechanisms.
- Never let C++ exceptions escape C callbacks. Translate them into the C API's error/abort mechanism and propagate failures at the C++ boundary; do not silently swallow them.
- Preserve hardening, zero auto-initialization, ASan/UBSan with fatal reports, and required lint/tests. These checks do not prove memory safety.
