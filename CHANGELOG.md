# Changelog

## Unreleased

- Bind HTTP server to 0.0.0.0 by default (--host 127.0.0.1 for loopback-only)
- Add --log-file flag with 10 MiB log rotation (shop-assistant-mcp.log.1)
- HTTP server no longer rejects non-loopback hosts, port 0, or foreign Host/Origin headers (slimr-mcp upgrade)
- Internal: align build, CI, Homebrew formula, and documentation with cpp-templates
