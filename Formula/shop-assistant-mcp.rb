class ShopAssistantMcp < Formula
  desc "Guarded retail shopping and browser automation MCP server over CDP"
  homepage "https://github.com/bdombro/shop-assistant-mcp"
  version "2.1.0"

  on_macos do
    url "https://github.com/bdombro/shop-assistant-mcp/releases/download/v2.1.0/shop-assistant-mcp-arm64-macos.tar.gz"
    sha256 "6ad582834839812552ee2119b4110fb7300f0ba61fda073f50a4c3bb05b607b8"
  end

  def install
    bin.install "shop-assistant-mcp"
    chmod 0755, bin/"shop-assistant-mcp"
    pkgshare.install "docs", "sites" if File.directory?("docs") && File.directory?("sites")
  end

  def post_install
    (var/"shop-assistant-mcp").mkpath
    chmod 0700, var/"shop-assistant-mcp"
    (var/"log").mkpath
  end

  service do
    run ["/bin/sh", "-c", "[ -f #{etc}/shop-assistant-mcp/env ] && set -a && . #{etc}/shop-assistant-mcp/env; exec #{opt_bin}/shop-assistant-mcp http --log-file #{var}/log/shop-assistant-mcp.log"]
    keep_alive true
    working_dir var/"shop-assistant-mcp"
    log_path var/"log/shop-assistant-mcp.log"
    error_log_path var/"log/shop-assistant-mcp.log"
    environment_variables PATH: "#{HOMEBREW_PREFIX}/bin:/usr/bin:/bin"
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/shop-assistant-mcp --version")
    discover = '{"jsonrpc":"2.0","id":1,"method":"server/discover","params":{"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}}'
    out = pipe_output("#{bin}/shop-assistant-mcp mcp --skip-checks", "#{discover}\n")
    assert_match "shop-assistant-mcp", out
  end
end
