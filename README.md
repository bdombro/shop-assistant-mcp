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

## Installation

- **Prebuilt release:** the checked-in [Homebrew formula](Formula/shop-assistant-mcp.rb)
  currently has a placeholder checksum. It is not a ready-to-install release.
  Check [releases](https://github.com/bdombro/shop-assistant-mcp/releases) for published
  binaries and a finalized formula before using a release installation command.
- **Build from source:** the current path is [Development](#development).
  Local slimr overlays are for development, not a runtime requirement.
- **Already installed:** follow the setup and service steps below. Installing a
  Cursor/Claude manifest only configures an endpoint; it does not install or
  start the native service.


## Configure the Shopping Browser

Install Brave with `brew install --cask brave-browser`. Interactive tools
require a Brave instance exposing a **loopback-only** CDP endpoint, normally
`http://127.0.0.1:9222`. Use a dedicated browser profile, not your everyday
signed-in profile; do not expose remote debugging to your network.

Set `SHOP_ASSISTANT_CDP_URL` if using another endpoint. Configure site
preferences in the private file described under
[Site Access Policy & Configuration](#site-access-policy--configuration).
The server manages its own tabs and refuses checkout/account pages.
macOS supplies the confirmation dialogs; unattended mode denies prompts.


## Start and Manage the Service

For an already-installed Homebrew formula:

```bash
brew services start bdombro/shop-assistant-mcp/shop-assistant-mcp
brew services info shop-assistant-mcp
brew services restart bdombro/shop-assistant-mcp/shop-assistant-mcp
brew services stop bdombro/shop-assistant-mcp/shop-assistant-mcp
```

The endpoint is `http://127.0.0.1:8085/mcp`; logs are under
`/opt/homebrew/var/log/`. To run in the foreground instead, use
`shop-assistant-mcp http` (or `./build/bin/shop-assistant-mcp http` from a source checkout).
Do not run the foreground server and service on the same port.
Keep the default loopback bind; this is not a shared remote-host deployment.


## Connect Your Agent

After starting the service, add this server to OpenCode v2.2.6's
`opencode.jsonc` (merge it with any existing servers):

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

Use either an explicit server entry or a plugin manifest, not both.
Alternative client schemas and stdio configuration are in the
[developer reference](docs/development.md). The compatibility notice above
applies to every transport.


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

## Troubleshooting

- **Client cannot connect:** confirm it supports July MCP, the service is
  running, and the client URL matches the configured port. OpenCode needs
  `"protocol": "2026-07-28"`; old `initialize` clients will fail.
- **Port occupied:** inspect the listener and stop only the service/process
  you own, or deliberately choose another port and update the client URL.
- **Works in a terminal, not as a service:** launchd does not inherit
  `~/.zshrc`. Configure the service environment separately or use the
  plugin's supported private configuration file; inspect
  `/opt/homebrew/var/log/` for errors.
- **Browser tools unavailable:** verify Brave's loopback CDP endpoint and dedicated profile. Handle CAPTCHA/access-denied pages manually; do not bypass them.
- **Source build cannot find vcpkg or slimr ports:** load the development
  environment, bootstrap vcpkg, and register both slimr links and the pinned
  libhv overlay. Never repair this by editing installed headers.
- **Compiler/hardening check fails:** upgrade the Xcode toolchain; set
  `CXX` before first configure. CMake caches the compiler, so an intentional
  compiler change needs a fresh configuration tree.


## Development

This section is for building or changing the server, not configuring an
installed binary. [Architecture, transport reference and test details](docs/development.md)
are separate from the user setup flow.

<details>
<summary>macOS source-build setup (vcpkg and local slimr overlays)</summary>

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

</details>

After the source build succeeds, stage and install the local Homebrew formula:

```bash
just install-local
```

Installing a formula and starting its service are separate actions.
For a source-only foreground run, use
`./build/bin/shop-assistant-mcp http`.
