# AGENTS.md (`shop-assistant-mcp`)

Written for: AI coding agents working in the `shop-assistant-mcp` C++26 plugin repository.

`shop-assistant-mcp` is a C++26 MCP server built on `slimr-*` libraries and vcpkg-managed Lexbor that lets agents search, compare offers, read product pages, and interact with guarded Brave CDP browser tabs across 22 onboarded retail sites without ever being able to purchase or access checkout/account pages.

<!-- cpp-templates:begin -->
<!-- cpp-templates:commit c746b02c9f5b17b63c4921e6adff7fa8c5f6dc37 -->
## Contributing

Read the sections of [docs/contributing.md](docs/contributing.md) relevant to your change before editing.

## Tooling alignment

Align this repository via the `cpp-templates` skill, which diffs `base/` and `templates/<template>/` since the `<!-- cpp-templates:commit <sha> -->` marker above and adapts changes while preserving repository customizations. Keep repository-specific rules outside `<!-- cpp-templates:begin -->` and `<!-- cpp-templates:end -->`.

## Edit-don't-rewrite

- Make targeted edits and move files with `git mv`; never regenerate files from scratch or retype them from memory.
- Never delete, skip, or loosen tests to pass; if a test must change, explain why in the commit message.

## C++ Memory Safety

- Immediately wrap owned C handles, allocations, descriptors, and temporary resources in RAII. Make owning wrappers noncopyable unless copying deliberately creates independent ownership; moves must leave the source safely destructible.
- Borrow views, pointers, and references only for the current call by default. Store owning values unless lifetime/invalidation guarantees are explicit. Deferred callbacks require explicit ownership-safe captures; no escaping reference captures without a demonstrated lifetime guarantee.
- Initialize scalar/pointer members at declaration and local values before use. Compiler zero-initialization is defense in depth, not permission for uninitialized reads.
- Check range/overflow before narrowing lengths/counts at C API boundaries; casts alone are not validation. Report failures through existing error mechanisms.
- Never let C++ exceptions escape C callbacks. Translate them into the C API's error/abort mechanism and propagate failures at the C++ boundary; do not silently swallow them.
- Preserve libc++ hardening, zero auto-initialization, ASan/UBSan with fatal reports, and required lint/tests. These checks do not prove memory safety.

## C++ style

- Favor modern C++26 and the standard library over legacy patterns; write for readability; comment what is not obvious and why.
- Prefer values over pointers, `const` by default, and no raw `new`/`delete`.
- Use `std::expected` for expected failures; use exceptions only at process and callback boundaries.
- Structure code with one folder per feature under `src/`, mirrored under `tests/`. Keep one test TU per executable (`tests/test_main.cpp`).
- Name what you use (`using slimr::Json;` or `slimr::Json`, never `using namespace`). Use PascalCase for types and camelCase for functions and variables (review guidance, not lint).
- Put file-local helpers in an unnamed namespace in `.cpp` files, or in `detail::` in headers.
- Follow KISS: inline single-use helpers, avoid speculative abstractions, keep diffs lean.
- Apply style rules to new and touched code; ports do not rewrite working code just for style.
- `.clang-tidy` enforces bug-finding checks (`clang-analyzer-core.*`, `clang-analyzer-cplusplus.*`, and bug-risk `bugprone-*` checks; pending legacy findings are temporarily disabled with per-check counts in `.clang-tidy`); fix findings or suppress inline with a reason.
- `INTERFACE_SOURCES` caveat: linking a header-only library that ships an implementation TU (such as `slimr-yaml`) into both a library and an executable that links that library compiles the TU twice and causes duplicate symbols.

## Testing

Iterate with `just test "<case>"` (Debug build with ASan + UBSan, `BUGSINK_DSN=""`), then finish with a full `just test`. `just build` compiles Release only and runs no tests.

## Recipes

- Core: `build`, `clean`, `configure`, `deps`, `doctor`, `fmt`, `lint`, `test [filter]`, `upgrade-deps`
- `homebrew-*`: `install`, `uninstall`
- `homebrew-service` and `homebrew-mcp`: `daemon-logs`, `daemon-restart`, `daemon-start`, `daemon-status`, `daemon-stop`
- `homebrew-mcp`: `test-mcp`
- Dropped tooling (do not re-add): foreground runner, local tap/overlay symlinking, GitHub-branch/local-publish/dry-run helpers, standalone formula check, `release` recipe, and JS formula helpers

## Changelog

PRs touch no release files (`CHANGELOG.md`, `VERSION`, `versions/`). Write the PR title (and optional body) as the changelog entry; `release.yml` generates `CHANGELOG.md` from squash-merge commits after merge.

## Paths, env vars, and README

- `<name>` in Homebrew paths is always the formula name, never a short XDG name: app data lives in `$(brew --prefix)/var/<name>`, config in `$(brew --prefix)/etc/<name>/`, and service secrets in `$(brew --prefix)/etc/<name>/env` (`chmod 600`, never in the public formula).
- Secrets are env vars only.
- Every `README.md` is end-user first: what it does, install, usage, env var table (name, required or optional, purpose), then links into `docs/` (`docs/contributing.md`, `docs/architecture.md`) for contributor workflow and architecture.
<!-- cpp-templates:end -->


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

CMake declares targets/dependencies; Ninja owns incremental builds. Just orchestrates automatic `deps` before configure/build/test/lint; CMake manifest auto-install is disabled. Set `VCPKG_ROOT` in the environment. Dependencies install under `build/deps/`; Debug sanitizer and Release trees live under `build/cmake/`. Use installed `slimr-*` targets from the pinned vcpkg registries, never source-checkout include overrides. SQLite with FTS5 and Lexbor use their static imported targets.

Use `llvm@23` (`clang++` with `-stdlib=libc++` and static `libc++.a` + `libc++abi.a`, `LLVM_BIN`) with C++26 and libc++ FAST hardening, CMake, Ninja, vcpkg, and just. CTest runs fixture tests from the repository root with `SHOP_ASSISTANT_DIALOGS=off`. `test-mcp` checks July discovery over ephemeral HTTP with Bun and an isolated server environment; never call browser/data tools in CI.
