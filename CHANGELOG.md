# Changelog

## Unreleased

- Internal: migrate HTTP/WebSocket client and URL parser to cpp-httplib and RFC 3986 parser
- Internal: switch macOS builds to `llvm@23` and static `libc++`

## v2.1.0 - 2026-11-16

- Tag Bugsink error reports with shop-assistant-mcp@<version> from VERSION
- Bind HTTP server to 0.0.0.0 by default (--host 127.0.0.1 for loopback-only)
- Add --log-file flag with 10 MiB log rotation (shop-assistant-mcp.log.1)
- HTTP server no longer rejects non-loopback hosts, port 0, or foreign Host/Origin headers (slimr-mcp upgrade)
- Internal: align build, CI, Homebrew formula, and documentation with cpp-templates
- Internal: resolve slimr registries from the `slimr-down-now` org
