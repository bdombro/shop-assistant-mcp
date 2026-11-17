class ShopAssistantMcp < Formula
  desc "Guarded retail shopping and browser automation MCP server over CDP"
  homepage "https://github.com/bdombro/shop-assistant-mcp"
  version "2.2.2"

  on_macos do
    url "https://github.com/bdombro/shop-assistant-mcp/releases/download/v2.2.2/shop-assistant-mcp-arm64-macos.tar.gz"
    sha256 "8328b91cd5c2526f931ed7d078b980caec2d0cc72fe6e3c7aa78d37281dc3cec"
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
    require "net/http"
    assert_match version.to_s, shell_output("#{bin}/shop-assistant-mcp --version")
    r, w = IO.pipe
    pid = spawn("#{bin}/shop-assistant-mcp", "--skip-checks", "http", "--host", "127.0.0.1", "--port", "0", err: w)
    w.close
    begin
      port = nil
      r.each_line { |line| break if (port = line[%r{listening on http://[\d.]+:(\d+)/mcp}, 1]) }
      assert port, "server did not report a listening port"
      discover = '{"jsonrpc":"2.0","id":1,"method":"server/discover","params":{"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}}'
      res = Net::HTTP.post(URI("http://127.0.0.1:#{port}/mcp"), discover,
                           "Content-Type"         => "application/json",
                           "Accept"               => "application/json, text/event-stream",
                           "MCP-Protocol-Version" => "2026-07-28",
                           "Mcp-Method"           => "server/discover")
      assert_match "shop-assistant-mcp", res.body
    ensure
      Process.kill("TERM", pid)
      Process.wait(pid)
    end
  end
end
