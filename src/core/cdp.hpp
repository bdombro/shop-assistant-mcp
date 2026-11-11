#pragma once

#include "curl.hpp"
#include "json.hpp"
#include "ws.hpp"

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace shop::core {

inline std::string braveUnreachable(int port) {
    return "Brave isn't reachable on 127.0.0.1:" + std::to_string(port) +
           ". Ask the user to start Brave with --remote-debugging-port=" + std::to_string(port) + ".";
}

const std::string CDP_CLOSED =
    "the connection to Brave dropped (Brave restarted or the Mac slept); retry the tool call: it reconnects";

struct CdpEvent {
    std::string method;
    slimr::Json params;
    std::optional<std::string> sessionId;
};

class CdpClientInterface {
public:
    virtual ~CdpClientInterface() = default;

    virtual slimr::Json send(
        std::string_view method,
        const slimr::Json& params = slimr::Json{},
        std::optional<std::string_view> sessionId = std::nullopt) = 0;

    virtual size_t on(std::function<void(const CdpEvent&)> listener) = 0;
    virtual void off(size_t id) = 0;
    virtual void onClose(std::function<void()> listener) = 0;
    virtual bool isOpen() const noexcept = 0;
    virtual void close() = 0;

    // High-level CDP operations

    std::string createTarget(std::string_view url = "about:blank", bool background = true) {
        slimr::Json::object_t params;
        params["url"] = std::string(url);
        params["background"] = background;
        auto res = send("Target.createTarget", slimr::Json(std::move(params)));
        if (res.is_object() && res.contains("targetId") && res["targetId"].is_string()) {
            return res["targetId"].get<std::string>();
        }
        throw std::runtime_error("createTarget failed: targetId missing in response");
    }

    void closeTarget(std::string_view targetId) {
        slimr::Json::object_t params;
        params["targetId"] = std::string(targetId);
        (void)send("Target.closeTarget", slimr::Json(std::move(params)));
    }

    std::string attachToTarget(std::string_view targetId, bool flatten = true) {
        slimr::Json::object_t params;
        params["targetId"] = std::string(targetId);
        params["flatten"] = flatten;
        auto res = send("Target.attachToTarget", slimr::Json(std::move(params)));
        if (res.is_object() && res.contains("sessionId") && res["sessionId"].is_string()) {
            return res["sessionId"].get<std::string>();
        }
        throw std::runtime_error("attachToTarget failed: sessionId missing in response");
    }

    std::vector<slimr::Json> getTargets() {
        auto res = send("Target.getTargets");
        if (res.is_object() && res.contains("targetInfos") && res["targetInfos"].is_array()) {
            return res["targetInfos"].get<slimr::Json::array_t>();
        }
        return {};
    }

    std::string navigate(std::string_view sessionId, std::string_view url) {
        slimr::Json::object_t params;
        params["url"] = std::string(url);
        auto res = send("Page.navigate", slimr::Json(std::move(params)), sessionId);
        if (res.is_object()) {
            if (res.contains("errorText") && res["errorText"].is_string()) {
                throw std::runtime_error("navigation failed: " + res["errorText"].get<std::string>());
            }
            if (res.contains("frameId") && res["frameId"].is_string()) {
                return res["frameId"].get<std::string>();
            }
        }
        return "";
    }

    slimr::Json evaluate(
        std::string_view sessionId,
        std::string_view expression,
        bool returnByValue = true,
        bool awaitPromise = true) {
        slimr::Json::object_t params;
        params["expression"] = std::string(expression);
        params["returnByValue"] = returnByValue;
        params["awaitPromise"] = awaitPromise;
        return send("Runtime.evaluate", slimr::Json(std::move(params)), sessionId);
    }

    std::string getOuterHtml(std::string_view sessionId, int64_t nodeId = 0) {
        if (nodeId == 0) {
            auto evalRes = evaluate(sessionId, "document.documentElement.outerHTML", true, false);
            if (evalRes.is_object() && evalRes.contains("result")) {
                const auto& r = evalRes["result"];
                if (r.is_object() && r.contains("value") && r["value"].is_string()) {
                    return r["value"].get<std::string>();
                }
            }
            return "";
        }
        slimr::Json::object_t params;
        params["nodeId"] = nodeId;
        auto res = send("DOM.getOuterHTML", slimr::Json(std::move(params)), sessionId);
        if (res.is_object() && res.contains("outerHTML") && res["outerHTML"].is_string()) {
            return res["outerHTML"].get<std::string>();
        }
        return "";
    }

    slimr::Json getFullAXTree(std::string_view sessionId) {
        return send("Accessibility.getFullAXTree", slimr::Json{}, sessionId);
    }

    void dispatchKeyEvent(
        std::string_view sessionId,
        std::string_view type,
        std::string_view text = "",
        std::string_view key = "") {
        slimr::Json::object_t params;
        params["type"] = std::string(type);
        if (!text.empty()) params["text"] = std::string(text);
        if (!key.empty()) params["key"] = std::string(key);
        (void)send("Input.dispatchKeyEvent", slimr::Json(std::move(params)), sessionId);
    }

    std::string captureScreenshot(
        std::string_view sessionId,
        std::string_view format = "png",
        int quality = 80) {
        slimr::Json::object_t params;
        params["format"] = std::string(format);
        if (format == "jpeg") params["quality"] = quality;
        auto res = send("Page.captureScreenshot", slimr::Json(std::move(params)), sessionId);
        if (res.is_object() && res.contains("data") && res["data"].is_string()) {
            return res["data"].get<std::string>();
        }
        throw std::runtime_error("captureScreenshot failed");
    }

    // Fetch Interception Guard

    void enableGuard(std::string_view sessionId) {
        slimr::Json::object_t params;
        slimr::Json::array_t patterns;
        slimr::Json::object_t pat;
        pat["urlPattern"] = "*";
        patterns.push_back(std::move(pat));
        params["patterns"] = std::move(patterns);
        (void)send("Fetch.enable", slimr::Json(std::move(params)), sessionId);
    }

    void continueRequest(std::string_view sessionId, std::string_view requestId) {
        slimr::Json::object_t params;
        params["requestId"] = std::string(requestId);
        (void)send("Fetch.continueRequest", slimr::Json(std::move(params)), sessionId);
    }

    void failRequest(
        std::string_view sessionId,
        std::string_view requestId,
        std::string_view errorReason = "AccessDenied") {
        slimr::Json::object_t params;
        params["requestId"] = std::string(requestId);
        params["errorReason"] = std::string(errorReason);
        (void)send("Fetch.failRequest", slimr::Json(std::move(params)), sessionId);
    }

    void fulfillRequest(
        std::string_view sessionId,
        std::string_view requestId,
        int responseCode,
        const std::map<std::string, std::string>& headers = {},
        std::string_view body = "") {
        slimr::Json::object_t params;
        params["requestId"] = std::string(requestId);
        params["responseCode"] = responseCode;
        if (!headers.empty()) {
            slimr::Json::array_t headerList;
            for (const auto& [k, v] : headers) {
                slimr::Json::object_t h;
                h["name"] = k;
                h["value"] = v;
                headerList.push_back(std::move(h));
            }
            params["responseHeaders"] = std::move(headerList);
        }
        if (!body.empty()) {
            params["body"] = std::string(body);
        }
        (void)send("Fetch.fulfillRequest", slimr::Json(std::move(params)), sessionId);
    }
};

class CdpClient : public CdpClientInterface {
public:
    CdpClient() = default;

    static std::unique_ptr<CdpClient> connect(int port, long timeoutMs = 5000) {
        std::string url = "http://127.0.0.1:" + std::to_string(port) + "/json/version";
        slimr::HttpRequest req;
        req.url = url;
        req.timeoutMs = timeoutMs;
        slimr::HttpResponse resp;
        try {
            resp = slimr::request(req);
        } catch (...) {
            throw std::runtime_error(braveUnreachable(port));
        }
        if (resp.status != 200) {
            throw std::runtime_error(braveUnreachable(port));
        }
        auto parsed = slimr::parse_json<slimr::Json>(resp.body);
        if (!parsed || !parsed->is_object() || !parsed->contains("webSocketDebuggerUrl")) {
            throw std::runtime_error(braveUnreachable(port));
        }
        std::string wsUrl = (*parsed)["webSocketDebuggerUrl"].get<std::string>();

        auto client = std::make_unique<CdpClient>();
        if (!client->ws_.connect(wsUrl, {}, timeoutMs)) {
            throw std::runtime_error(braveUnreachable(port));
        }
        client->is_open_ = true;
        return client;
    }

    slimr::Json send(
        std::string_view method,
        const slimr::Json& params = slimr::Json{},
        std::optional<std::string_view> sessionId = std::nullopt) override {
        if (!is_open_ || !ws_.isConnected()) {
            shutdown();
            throw std::runtime_error(CDP_CLOSED);
        }

        int64_t id = ++seq_;
        slimr::Json::object_t req;
        req["id"] = id;
        req["method"] = std::string(method);
        if (!params.is_null()) {
            req["params"] = params;
        }
        if (sessionId.has_value()) {
            req["sessionId"] = std::string(*sessionId);
        }

        if (!ws_.sendJson(slimr::Json(std::move(req)))) {
            shutdown();
            throw std::runtime_error(CDP_CLOSED);
        }

        auto start = std::chrono::steady_clock::now();
        while (is_open_) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed > 30000) {
                throw std::runtime_error("CDP " + std::string(method) + " got no answer within 30s");
            }

            auto msg = ws_.recvJson(500);
            if (!msg) {
                if (!ws_.isConnected()) {
                    shutdown();
                    throw std::runtime_error(CDP_CLOSED);
                }
                continue;
            }

            if (msg->is_object()) {
                if (msg->contains("id")) {
                    int64_t respId = (*msg)["id"].get<int64_t>();
                    if (respId == id) {
                        if (msg->contains("error") && !(*msg)["error"].is_null()) {
                            const auto& err = (*msg)["error"];
                            std::string errMsg = err.contains("message") ? err["message"].get<std::string>() : "unknown error";
                            if (err.contains("data") && err["data"].is_string()) {
                                errMsg += ": " + err["data"].get<std::string>();
                            }
                            throw std::runtime_error(errMsg);
                        }
                        if (msg->contains("result")) {
                            return (*msg)["result"];
                        }
                        return slimr::Json{};
                    }
                } else if (msg->contains("method")) {
                    CdpEvent ev;
                    ev.method = (*msg)["method"].get<std::string>();
                    if (msg->contains("params")) ev.params = (*msg)["params"];
                    if (msg->contains("sessionId") && (*msg)["sessionId"].is_string()) {
                        ev.sessionId = (*msg)["sessionId"].get<std::string>();
                    }
                    dispatchListeners(ev);
                }
            }
        }
        throw std::runtime_error(CDP_CLOSED);
    }

    size_t on(std::function<void(const CdpEvent&)> listener) override {
        size_t id = ++listener_seq_;
        listeners_[id] = std::move(listener);
        return id;
    }

    void off(size_t id) override {
        listeners_.erase(id);
    }

    void onClose(std::function<void()> listener) override {
        if (!is_open_) {
            listener();
        } else {
            close_listeners_.push_back(std::move(listener));
        }
    }

    bool isOpen() const noexcept override {
        return is_open_ && ws_.isConnected();
    }

    void close() override {
        shutdown();
    }

private:
    slimr::WebSocket ws_;
    int64_t seq_ = 0;
    size_t listener_seq_ = 0;
    bool is_open_ = false;
    std::map<size_t, std::function<void(const CdpEvent&)>> listeners_;
    std::vector<std::function<void()>> close_listeners_;

    void dispatchListeners(const CdpEvent& ev) {
        auto copy = listeners_;
        for (const auto& [_, l] : copy) {
            l(ev);
        }
    }

    void shutdown() {
        if (!is_open_) return;
        is_open_ = false;
        ws_.close();
        auto closeCopy = std::move(close_listeners_);
        close_listeners_.clear();
        for (const auto& l : closeCopy) {
            l();
        }
    }
};

} // namespace shop::core
