class ShopAssistantMcp < Formula
  desc "Guarded retail shopping and browser automation MCP server over CDP"
  homepage "https://github.com/bdombro/shop-assistant-mcp"
  version "2.0.0"
  license "ISC"

  url "https://github.com/bdombro/shop-assistant-mcp/releases/download/v2.0.0/shop-assistant-mcp-v2.0.0-darwin-arm64.tar.gz"
  sha256 "0000000000000000000000000000000000000000000000000000000000000000"

  def install
    bin.install "shop-assistant-mcp"
    pkgshare.install "docs", "sites" if File.directory?("docs") && File.directory?("sites")
  end

  service do
    run [opt_bin/"shop-assistant-mcp", "http", "--host", "127.0.0.1", "--port", "8085"]
    working_dir opt_pkgshare
    keep_alive true
    log_path var/"log/shop-assistant-mcp.log"
    error_log_path var/"log/shop-assistant-mcp.log"
  end

  def caveats
    <<~EOS
      HTTP MCP endpoint:
        http://127.0.0.1:8085/mcp

      Optional user policy & Brave CDP configuration:
        ~/.config/shop-assistant/config.json
      Interactive browser tab tools connect to Brave at http://127.0.0.1:9222
      (--remote-debugging-port=9222).
    EOS
  end

  test do
    output = shell_output("#{bin}/shop-assistant-mcp status")
    assert_match "shop-assistant-mcp", output
  end
end
