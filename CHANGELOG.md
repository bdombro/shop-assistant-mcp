# Changelog

## Unreleased

- Default HTTP bind is now `127.0.0.1` (pass `--host 0.0.0.0` to expose) via `slimr-serve` v0.3.0
- Serve OpenAPI 3.1 spec at `GET /openapi.json` and dump via `--openapi` (`slimr-serve` v0.2.0+, `slimr-mcp` v0.2.1)
- Internal: pin `slimr-serve` v0.3.0 and `slimr-mcp` v0.2.1 baselines, sync `scripts/test-mcp.ts`, and document overlay-ports workflow

## v2.2.0 - 2026-11-16

- Migrate to slimr-serve typed tool handlers and drop stdio `mcp` mode
- Internal: migrate HTTP/WebSocket client and URL parser to cpp-httplib and RFC 3986 parser
- Internal: switch macOS builds to `llvm@23` and static `libc++`
- Internal: pin slimr-mcp v0.2.0 and slimr-serve v0.1.1 baselines, drop overlay-ports

## v2.1.0 - 2026-11-16

- Tag Bugsink error reports with shop-assistant-mcp@<version> from VERSION
- Bind HTTP server to 0.0.0.0 by default (--host 127.0.0.1 for loopback-only)
- Add --log-file flag with 10 MiB log rotation (shop-assistant-mcp.log.1)
- HTTP server no longer rejects non-loopback hosts, port 0, or foreign Host/Origin headers (slimr-mcp upgrade)
- Internal: align build, CI, Homebrew formula, and documentation with cpp-templates
- Internal: resolve slimr registries from the `slimr-down-now` org
