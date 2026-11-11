#pragma once

#include "markdown.hpp"
#include "policy.hpp"
#include "sqlite.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace shop::core {

inline int defaultTtlS(PageKind kind) {
    switch (kind) {
        case PageKind::search: return 3600;
        case PageKind::product: return 4 * 3600;
        case PageKind::reviews: return 24 * 3600;
        case PageKind::offers: return 3600;
        case PageKind::stock: return 3600;
        case PageKind::departments: return 7 * 24 * 3600;
        case PageKind::policy: return 7 * 24 * 3600;
        case PageKind::other: return 3600;
    }
}

struct GuardEvent {
    int64_t at = 0;
    int64_t pageId = 0;
    std::optional<std::string> site;
    std::string method;
    std::string url;
    std::string action;   // "continue" | "ask" | "fail"
    std::string outcome;  // "allowed" | "denied" | "blocked" | "timeout"
    std::string reason;
};

struct CachedPage : public CleanPage {
    int64_t fetchedAt = 0;
};

struct SiteHealth {
    std::optional<int64_t> lastChecked;
    std::optional<int64_t> lastWorked;
    std::optional<std::string> lastError;
};

const int SCHEMA_VERSION = 1;

const std::string SCHEMA_SQL = R"(
CREATE TABLE IF NOT EXISTS pages (
  key TEXT PRIMARY KEY,
  kind TEXT NOT NULL,
  url TEXT NOT NULL,
  fetched_at INTEGER NOT NULL,
  expires_at INTEGER NOT NULL,
  body TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS guard_log (
  at INTEGER NOT NULL,
  page_id INTEGER NOT NULL,
  site TEXT,
  method TEXT NOT NULL,
  url TEXT NOT NULL,
  action TEXT NOT NULL,
  outcome TEXT NOT NULL,
  reason TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS guard_log_page ON guard_log (page_id, at);
CREATE TABLE IF NOT EXISTS site_checks (
  site TEXT NOT NULL,
  at INTEGER NOT NULL,
  ok INTEGER NOT NULL,
  detail TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS site_checks_site ON site_checks (site, at);
)";

class Store {
public:
    Store(std::unique_ptr<slimr::SqliteDb> db, std::function<int64_t()> now_fn = nullptr)
        : db_(std::move(db)), now_(now_fn ? std::move(now_fn) : defaultNow) {}

    static int64_t defaultNow() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    static Store open(const std::string& path, std::function<int64_t()> now_fn = nullptr) {
        slimr::SqliteOptions opts;
        opts.migrations = std::map<int, std::string>{{SCHEMA_VERSION, SCHEMA_SQL}};
        auto db = std::make_unique<slimr::SqliteDb>(path, std::move(opts));
        return Store(std::move(db), std::move(now_fn));
    }

    slimr::SqliteDb& sqlite() { return *db_; }

    std::optional<CachedPage> getPage(std::string_view key) {
        auto row = db_->get("SELECT fetched_at, expires_at, body FROM pages WHERE key = ?", {std::string(key)});
        if (!row || row->is_null()) return std::nullopt;
        int64_t expiresAt = (*row)["expires_at"].get<int64_t>();
        if (expiresAt <= now_()) return std::nullopt;

        int64_t fetchedAt = (*row)["fetched_at"].get<int64_t>();
        std::string body = (*row)["body"].get<std::string>();
        auto parsed = slimr::parse_json<CleanPage>(body);
        if (!parsed) return std::nullopt;

        CachedPage res;
        static_cast<CleanPage&>(res) = std::move(*parsed);
        res.fetchedAt = fetchedAt;
        return res;
    }

    void putPage(std::string_view key, PageKind kind, int64_t ttlS, const CleanPage& page) {
        int64_t fetchedAt = now_();
        int64_t expiresAt = fetchedAt + ttlS * 1000;
        std::string body = slimr::write_json(page);
        db_->run(
            "INSERT OR REPLACE INTO pages (key, kind, url, fetched_at, expires_at, body) VALUES (?, ?, ?, ?, ?, ?)",
            {std::string(key), pageKindToString(kind), page.url, fetchedAt, expiresAt, std::move(body)});
    }

    void logGuard(const GuardEvent& event) {
        slimr::SqlValue siteVal;
        if (event.site.has_value()) {
            siteVal = *event.site;
        } else {
            siteVal = std::monostate{};
        }
        db_->run(
            "INSERT INTO guard_log (at, page_id, site, method, url, action, outcome, reason) VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
            {event.at, event.pageId, siteVal, event.method, event.url, event.action, event.outcome, event.reason});
    }

    std::vector<GuardEvent> guardEvents(const std::vector<int64_t>& pageIds, int64_t sinceMs) {
        if (pageIds.empty()) return {};
        std::string sql = "SELECT at, page_id, site, method, url, action, outcome, reason FROM guard_log WHERE page_id IN (";
        std::vector<slimr::SqlValue> params;
        for (size_t i = 0; i < pageIds.size(); ++i) {
            if (i > 0) sql += ", ";
            sql += "?";
            params.push_back(pageIds[i]);
        }
        sql += ") AND at > ? ORDER BY at, rowid";
        params.push_back(sinceMs);

        auto rows = db_->all(sql, params);
        std::vector<GuardEvent> events;
        for (const auto& r : rows) {
            GuardEvent ev;
            ev.at = r["at"].get<int64_t>();
            ev.pageId = r["page_id"].get<int64_t>();
            if (r.contains("site") && !r["site"].is_null()) {
                ev.site = r["site"].get<std::string>();
            }
            ev.method = r["method"].get<std::string>();
            ev.url = r["url"].get<std::string>();
            ev.action = r["action"].get<std::string>();
            ev.outcome = r["outcome"].get<std::string>();
            ev.reason = r["reason"].get<std::string>();
            events.push_back(std::move(ev));
        }
        return events;
    }

    void recordCheck(std::string_view site, bool ok, std::string_view detail) {
        db_->run(
            "INSERT INTO site_checks (site, at, ok, detail) VALUES (?, ?, ?, ?)",
            {std::string(site), now_(), ok ? int64_t(1) : int64_t(0), std::string(detail)});
    }

    SiteHealth siteHealth(std::string_view site) {
        auto times = db_->get(
            "SELECT MAX(at) AS checked, MAX(CASE WHEN ok = 1 THEN at END) AS worked FROM site_checks WHERE site = ?",
            {std::string(site)});
        auto failure = db_->pluck<std::string>(
            "SELECT detail FROM site_checks WHERE site = ? AND ok = 0 ORDER BY at DESC, rowid DESC LIMIT 1",
            {std::string(site)});

        SiteHealth health;
        if (times && !times->is_null()) {
            if (times->contains("checked") && !(*times)["checked"].is_null()) {
                health.lastChecked = (*times)["checked"].get<int64_t>();
            }
            if (times->contains("worked") && !(*times)["worked"].is_null()) {
                health.lastWorked = (*times)["worked"].get<int64_t>();
            }
        }
        if (failure.has_value()) {
            health.lastError = *failure;
        }
        return health;
    }

    void close() {
        if (db_) {
            db_->close();
        }
    }

private:
    std::unique_ptr<slimr::SqliteDb> db_;
    std::function<int64_t()> now_;
};

} // namespace shop::core
