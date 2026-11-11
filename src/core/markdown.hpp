#pragma once

#include "dom.hpp"
#include "policy.hpp"
#include "json.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace shop::core {

struct Section {
    std::string id;
    std::string heading;
    std::string markdown;
    int tokens = 0;
};

struct CleanPage {
    std::string title;
    std::string url;
    std::vector<slimr::Json> jsonLd;
    std::vector<Section> sections;
};

struct OutlineEntry {
    std::string id;
    std::string heading;
    int tokens = 0;
    bool withheld = false;
};

struct PageView {
    std::vector<OutlineEntry> outline;
    int outlineOmitted = 0;
    std::string markdown;
    std::vector<slimr::Json> jsonLd;
    int jsonLdOmitted = 0;
    bool truncated = false;
    std::optional<std::string> cut;
};

struct PageViewOptions {
    std::vector<std::string> withheld;
    std::optional<std::vector<std::string>> only;
};

inline int estimateTokens(std::string_view text) {
    return static_cast<int>(std::ceil(static_cast<double>(text.size()) / 4.0));
}

inline std::string stripInvisible(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size();) {
        unsigned char c = text[i];
        if (c == 0xE2 && i + 2 < text.size()) {
            unsigned char c1 = text[i + 1];
            unsigned char c2 = text[i + 2];
            if ((c1 == 0x80 && (c2 >= 0x8B && c2 <= 0x8D)) || (c1 == 0x81 && c2 == 0xA0)) {
                i += 3;
                continue;
            }
        } else if (c == 0xEF && i + 2 < text.size()) {
            unsigned char c1 = text[i + 1];
            unsigned char c2 = text[i + 2];
            if (c1 == 0xBB && c2 == 0xBF) {
                i += 3;
                continue;
            }
        }
        out.push_back(text[i]);
        i++;
    }
    return out;
}

inline bool isBannerLike(const Node& el) {
    static const std::regex bannerRe(R"(cookie|consent|gdpr)", std::regex::icase);
    auto id = el.attr("id");
    if (id && std::regex_search(*id, bannerRe)) return true;
    auto cls = el.attr("class");
    if (cls && std::regex_search(*cls, bannerRe)) return true;
    return false;
}

inline bool isHidden(const Node& el) {
    if (el.hasAttr("hidden") || el.hasAttr("data-sa-hidden")) return true;
    auto ariaHidden = el.attr("aria-hidden");
    if (ariaHidden) {
        std::string ah = *ariaHidden;
        while (!ah.empty() && (ah.front() == ' ' || ah.front() == '\t')) ah.erase(ah.begin());
        while (!ah.empty() && (ah.back() == ' ' || ah.back() == '\t')) ah.pop_back();
        for (char& c : ah) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (ah == "true" && el.text().size() > 40) return true;
    }
    auto styleAttr = el.attr("style");
    if (!styleAttr) return false;
    std::string style = stripInvisible(*styleAttr);
    for (char& c : style) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    style.erase(std::remove_if(style.begin(), style.end(), [](char ch) {
        return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
    }), style.end());

    if (style.empty()) return false;
    if (std::regex_search(style, std::regex(R"((^|;)display:none)"))) return true;
    if (std::regex_search(style, std::regex(R"((^|;)visibility:(hidden|collapse))"))) return true;
    if (std::regex_search(style, std::regex(R"((^|;)opacity:0(\.0+)?(;|!|$))"))) return true;
    if (std::regex_search(style, std::regex(R"((^|;)font-size:0(\.0+)?(px|em|rem|pt|%)?(;|!|$))"))) return true;
    if (std::regex_search(style, std::regex(R"((^|;)(left|top|right|bottom|text-indent):-\d{4,})"))) return true;
    if (std::regex_search(style, std::regex(R"((^|;)clip:rect\(0)"))) return true;
    if (std::regex_search(style, std::regex(R"((^|;)clip-path:inset\(50%)"))) return true;
    if (std::regex_search(style, std::regex(R"((^|;)(height|width):0(px)?(;|!|$))")) &&
        std::regex_search(style, std::regex(R"(overflow:hidden)"))) return true;

    return false;
}

inline void absolutize(Node& el, std::string_view attrName, std::string_view baseUrl) {
    auto val = el.attr(attrName);
    if (!val || val->empty()) return;
    std::string v = *val;
    while (!v.empty() && (v.front() == ' ' || v.front() == '\t')) v.erase(v.begin());
    while (!v.empty() && (v.back() == ' ' || v.back() == '\t')) v.pop_back();

    CURLU* u = curl_url();
    if (!u) return;
    if (curl_url_set(u, CURLUPART_URL, std::string(baseUrl).c_str(), 0) == CURLUE_OK &&
        curl_url_set(u, CURLUPART_URL, v.c_str(), 0) == CURLUE_OK) {
        char* resolved = nullptr;
        if (curl_url_get(u, CURLUPART_URL, &resolved, 0) == CURLUE_OK && resolved) {
            el.setAttr(attrName, resolved);
            curl_free(resolved);
        }
    }
    curl_url_cleanup(u);
}

inline std::string renderMarkdown(lxb_dom_node_t* raw, const std::shared_ptr<DocumentState>& state) {
    if (!raw) return "";
    if (raw->type == LXB_DOM_NODE_TYPE_TEXT) {
        size_t len = 0;
        lxb_char_t* txt = lxb_dom_node_text_content(raw, &len);
        if (!txt || len == 0) return "";
        return std::string(reinterpret_cast<const char*>(txt), len);
    }
    if (raw->type != LXB_DOM_NODE_TYPE_ELEMENT) {
        return "";
    }
    Node el(raw, state);
    std::string tag = el.tagName();

    if (tag == "script" || tag == "style" || tag == "noscript" || tag == "svg" || tag == "iframe" || tag == "template") {
        return "";
    }

    auto renderChildren = [&]() -> std::string {
        std::string inner;
        lxb_dom_node_t* child = raw->first_child;
        while (child) {
            inner += renderMarkdown(child, state);
            child = child->next;
        }
        return inner;
    };

    if (tag == "img") {
        auto alt = el.attr("alt");
        std::string altText = alt ? *alt : "";
        while (!altText.empty() && (altText.front() == ' ' || altText.front() == '\t')) altText.erase(altText.begin());
        while (!altText.empty() && (altText.back() == ' ' || altText.back() == '\t')) altText.pop_back();
        if (!altText.empty() && altText.size() <= 80) {
            return "[img: " + altText + "]";
        }
        return "";
    }

    if (tag == "a") {
        std::string content = renderChildren();
        auto href = el.attr("href");
        std::string h = href ? *href : "";
        while (!h.empty() && (h.front() == ' ' || h.front() == '\t')) h.erase(h.begin());
        while (!h.empty() && (h.back() == ' ' || h.back() == '\t')) h.pop_back();

        static const std::regex deadRe(R"(^(javascript:|data:|#))", std::regex::icase);
        if (h.empty() || std::regex_search(h, deadRe)) {
            return content;
        }
        return "[" + content + "](" + h + ")";
    }

    if (tag == "strong" || tag == "b") {
        std::string content = renderChildren();
        if (content.empty()) return "";
        return "**" + content + "**";
    }

    if (tag == "em" || tag == "i") {
        std::string content = renderChildren();
        if (content.empty()) return "";
        return "*" + content + "*";
    }

    if (tag == "code") {
        std::string content = renderChildren();
        if (content.empty()) return "";
        return "`" + content + "`";
    }

    if (tag == "pre") {
        std::string content = el.text();
        return "\n\n```\n" + content + "\n```\n\n";
    }

    if (tag == "h1") return "\n\n# " + renderChildren() + "\n\n";
    if (tag == "h2") return "\n\n## " + renderChildren() + "\n\n";
    if (tag == "h3") return "\n\n### " + renderChildren() + "\n\n";
    if (tag == "h4") return "\n\n#### " + renderChildren() + "\n\n";
    if (tag == "h5") return "\n\n##### " + renderChildren() + "\n\n";
    if (tag == "h6") return "\n\n###### " + renderChildren() + "\n\n";

    if (tag == "p" || tag == "div") {
        std::string content = renderChildren();
        return "\n\n" + content + "\n\n";
    }

    if (tag == "br") return "\n";
    if (tag == "hr") return "\n\n---\n\n";

    if (tag == "blockquote") {
        std::string inner = renderChildren();
        std::istringstream iss(inner);
        std::string line, quoted;
        while (std::getline(iss, line)) {
            quoted += "> " + line + "\n";
        }
        return "\n\n" + quoted + "\n\n";
    }

    if (tag == "ul") {
        std::string out = "\n\n";
        lxb_dom_node_t* child = raw->first_child;
        while (child) {
            if (child->type == LXB_DOM_NODE_TYPE_ELEMENT) {
                Node c(child, state);
                if (c.tagName() == "li") {
                    std::string item = renderMarkdown(child, state);
                    while (!item.empty() && (item.front() == '\n' || item.front() == ' ' || item.front() == '\t')) item.erase(item.begin());
                    while (!item.empty() && (item.back() == '\n' || item.back() == ' ' || item.back() == '\t')) item.pop_back();
                    out += "- " + item + "\n";
                }
            }
            child = child->next;
        }
        out += "\n";
        return out;
    }

    if (tag == "ol") {
        std::string out = "\n\n";
        int idx = 1;
        lxb_dom_node_t* child = raw->first_child;
        while (child) {
            if (child->type == LXB_DOM_NODE_TYPE_ELEMENT) {
                Node c(child, state);
                if (c.tagName() == "li") {
                    std::string item = renderMarkdown(child, state);
                    while (!item.empty() && (item.front() == '\n' || item.front() == ' ' || item.front() == '\t')) item.erase(item.begin());
                    while (!item.empty() && (item.back() == '\n' || item.back() == ' ' || item.back() == '\t')) item.pop_back();
                    out += std::to_string(idx++) + ". " + item + "\n";
                }
            }
            child = child->next;
        }
        out += "\n";
        return out;
    }

    if (tag == "li") {
        return renderChildren();
    }

    if (tag == "table") {
        std::vector<std::vector<std::string>> headers;
        std::vector<std::vector<std::string>> rows;
        for (const auto& tr : el.querySelectorAll("tr")) {
            std::vector<std::string> cells;
            bool isHeader = false;
            for (const auto& cell : tr.children()) {
                if (cell.tagName() == "th") {
                    isHeader = true;
                    cells.push_back(cell.text());
                } else if (cell.tagName() == "td") {
                    cells.push_back(cell.text());
                }
            }
            if (isHeader) {
                headers.push_back(std::move(cells));
            } else if (!cells.empty()) {
                rows.push_back(std::move(cells));
            }
        }
        if (headers.empty() && !rows.empty()) {
            headers.push_back(std::move(rows.front()));
            rows.erase(rows.begin());
        }
        if (!headers.empty()) {
            std::string table = "\n\n|";
            size_t numCols = headers.front().size();
            for (const auto& h : headers.front()) table += " " + h + " |";
            table += "\n|";
            for (size_t c = 0; c < numCols; ++c) table += " --- |";
            table += "\n";
            for (const auto& row : rows) {
                table += "|";
                for (size_t c = 0; c < numCols; ++c) {
                    std::string val = (c < row.size()) ? row[c] : "";
                    table += " " + val + " |";
                }
                table += "\n";
            }
            table += "\n";
            return table;
        }
        return renderChildren();
    }

    return renderChildren();
}

inline std::string tidy(std::string_view md) {
    std::string s = stripInvisible(md);

    std::string result;
    std::istringstream iss(s);
    std::string line;
    while (std::getline(iss, line)) {
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r')) {
            line.pop_back();
        }
        static const std::regex emptyHeadingRe(R"(^#{1,6}$)");
        if (std::regex_match(line, emptyHeadingRe)) {
            line.clear();
        }
        result += line + "\n";
    }

    static const std::regex multiNewlines(R"(\n{3,})");
    result = std::regex_replace(result, multiNewlines, "\n\n");

    while (!result.empty() && (result.front() == ' ' || result.front() == '\t' || result.front() == '\r' || result.front() == '\n')) {
        result.erase(result.begin());
    }
    while (!result.empty() && (result.back() == ' ' || result.back() == '\t' || result.back() == '\r' || result.back() == '\n')) {
        result.pop_back();
    }
    return result;
}

inline std::vector<Section> splitSections(std::string_view md) {
    struct Chunk {
        std::string heading;
        std::vector<std::string> lines;
    };
    std::vector<Chunk> chunks;
    chunks.push_back(Chunk{"(top)", {}});
    bool fenced = false;
    std::string mdStr(md);
    std::istringstream iss(mdStr);
    std::string line;
    static const std::regex fenceRe(R"(^\s*(```|~~~))");
    static const std::regex headingRe(R"(^#{1,3}\s+(.*?)\s*#*$)");

    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (std::regex_search(line, fenceRe)) {
            fenced = !fenced;
        }
        std::smatch m;
        if (!fenced && std::regex_match(line, m, headingRe)) {
            std::string h = m[1].str();
            chunks.push_back(Chunk{h.empty() ? "(untitled)" : h, {line}});
        } else {
            chunks.back().lines.push_back(line);
        }
    }

    std::vector<Section> sections;
    for (const auto& c : chunks) {
        std::string markdown;
        for (size_t i = 0; i < c.lines.size(); ++i) {
            markdown += c.lines[i];
            if (i + 1 < c.lines.size()) markdown += "\n";
        }
        while (!markdown.empty() && (markdown.front() == ' ' || markdown.front() == '\t' || markdown.front() == '\r' || markdown.front() == '\n')) {
            markdown.erase(markdown.begin());
        }
        while (!markdown.empty() && (markdown.back() == ' ' || markdown.back() == '\t' || markdown.back() == '\r' || markdown.back() == '\n')) {
            markdown.pop_back();
        }
        if (markdown.empty()) continue;
        Section s;
        s.id = "s" + std::to_string(sections.size() + 1);
        s.heading = c.heading;
        s.markdown = markdown;
        s.tokens = estimateTokens(markdown);
        sections.push_back(std::move(s));
    }
    return sections;
}

inline CleanPage cleanHtml(std::string_view html, std::string_view url, const std::vector<std::string>& strip = {}) {
    Document doc = Document::parse(html);
    for (const auto& sel : strip) {
        for (auto el : doc.querySelectorAll(sel)) {
            el.remove();
        }
    }

    auto jsonLd = doc.jsonLd();

    auto titleEl = doc.querySelector("title");
    auto h1El = doc.querySelector("h1");
    std::string titleText = titleEl ? titleEl->text() : (h1El ? h1El->text() : "");
    titleText = stripInvisible(titleText);
    static const std::regex multiWs(R"(\s+)");
    titleText = std::regex_replace(titleText, multiWs, " ");
    while (!titleText.empty() && titleText.front() == ' ') titleText.erase(titleText.begin());
    while (!titleText.empty() && titleText.back() == ' ') titleText.pop_back();

    doc.removeComments();

    for (auto el : doc.querySelectorAll("*")) {
        if (el.isConnected() && isHidden(el)) {
            el.remove();
        }
    }

    for (auto el : doc.querySelectorAll("script, style, noscript, svg, iframe, template")) {
        el.remove();
    }

    auto candidates = doc.querySelectorAll("main, article, [role='main']");
    auto bodyEl = doc.body();
    size_t bodyTextLen = bodyEl ? bodyEl.text().size() : 0;
    std::optional<Node> best;
    size_t bestLen = 0;
    for (const auto& c : candidates) {
        size_t l = c.text().size();
        if (!best.has_value() || l > bestLen) {
            best = c;
            bestLen = l;
        }
    }

    std::optional<Node> region;
    if (best.has_value() && bestLen >= static_cast<size_t>(bodyTextLen * 0.2)) {
        region = best;
    }

    Node root = region.has_value() ? *region : (bodyEl ? bodyEl : doc.documentElement());

    if (region.has_value()) {
        for (auto el : doc.querySelectorAll("*")) {
            if (!isBannerLike(el)) continue;
            if (el.contains(*region) || el.nodeName() == "BODY" || el == doc.documentElement()) continue;
            el.remove();
        }
        for (auto el : root.querySelectorAll("nav, footer, aside")) {
            el.remove();
        }
    } else {
        for (auto el : root.querySelectorAll("nav, footer, aside, header, [role='navigation']")) {
            if (el.nodeName() == "HEADER" && el.querySelectorAll("nav, [role='navigation'], ul").empty()) continue;
            el.remove();
        }
        for (auto el : root.querySelectorAll("*")) {
            if (isBannerLike(el)) el.remove();
        }
    }

    if (!root) {
        return CleanPage{titleText, std::string(url), std::move(jsonLd), {}};
    }

    for (auto el : root.querySelectorAll("[alt], [title]")) {
        auto alt = el.attr("alt");
        if (alt && alt->size() > 200) el.removeAttr("alt");
        auto title = el.attr("title");
        if (title && title->size() > 200) el.removeAttr("title");
    }

    for (auto el : root.querySelectorAll("[href]")) {
        absolutize(el, "href", url);
    }
    for (auto el : root.querySelectorAll("[src]")) {
        absolutize(el, "src", url);
    }

    std::string mdRaw = renderMarkdown(root.raw(), root.state());
    std::string md = tidy(mdRaw);

    return CleanPage{
        titleText,
        std::string(url),
        std::move(jsonLd),
        splitSections(md)
    };
}

inline slimr::Json compactJson(const slimr::Json& val) {
    if (val.is_string()) {
        std::string s = val.get<std::string>();
        static const std::regex tagRe(R"(<[a-z!/])", std::regex::icase);
        if (std::regex_search(s, tagRe)) {
            static const std::regex htmlTagRe(R"(<[^>]*>)");
            s = std::regex_replace(s, htmlTagRe, " ");
            static const std::regex nbspRe(R"(&nbsp;|&#160;)");
            s = std::regex_replace(s, nbspRe, " ");
            static const std::regex ampRe(R"(&amp;)");
            s = std::regex_replace(s, ampRe, "&");
            static const std::regex wsRe(R"(\s+)");
            s = std::regex_replace(s, wsRe, " ");
            while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
            while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
        }
        if (s.size() > 300) {
            s = s.substr(0, 300) + "\xE2\x80\xA6";
        }
        return slimr::Json(s);
    }
    if (val.is_array()) {
        slimr::Json::array_t arr;
        for (const auto& item : val.get<slimr::Json::array_t>()) {
            arr.push_back(compactJson(item));
        }
        return slimr::Json(std::move(arr));
    }
    if (val.is_object()) {
        slimr::Json::object_t obj;
        for (const auto& [k, v] : val.get<slimr::Json::object_t>()) {
            obj[k] = compactJson(v);
        }
        return slimr::Json(std::move(obj));
    }
    return val;
}

inline CleanPage cleanJson(const slimr::Json& json, std::string_view url) {
    slimr::Json compacted = compactJson(json);
    std::string markdown = slimr::write_json(compacted);
    CleanPage page;
    page.title = std::string(url);
    page.url = std::string(url);
    page.jsonLd = {};
    Section s;
    s.id = "s1";
    s.heading = "(top)";
    s.markdown = markdown;
    s.tokens = estimateTokens(markdown);
    page.sections.push_back(std::move(s));
    return page;
}

inline PageView pageView(
    const CleanPage& page,
    int maxTokens,
    const PageViewOptions& options = {}) {
    std::vector<slimr::Json> jsonLd;
    int used = 0;
    for (const auto& blockRaw : page.jsonLd) {
        slimr::Json block = compactJson(blockRaw);
        std::string jsonStr = slimr::write_json(block);
        int t = estimateTokens(jsonStr);
        if (used + t > static_cast<int>(maxTokens * 0.3)) break;
        jsonLd.push_back(std::move(block));
        used += t;
    }

    std::vector<OutlineEntry> entries;
    for (const auto& s : page.sections) {
        bool isWithheld = std::find(options.withheld.begin(), options.withheld.end(), s.id) != options.withheld.end();
        entries.push_back(OutlineEntry{s.id, s.heading, s.tokens, isWithheld});
    }

    int outlineBudget = static_cast<int>(maxTokens * 0.25);
    std::string entriesJson = slimr::write_json(entries);
    std::vector<OutlineEntry> candidates;
    if (estimateTokens(entriesJson) <= outlineBudget) {
        candidates = entries;
    } else {
        for (const auto& e : entries) {
            if (e.tokens >= 20) candidates.push_back(e);
        }
    }

    std::vector<OutlineEntry> outline;
    int outlineUsed = 0;
    for (const auto& entry : candidates) {
        std::string entryJson = slimr::write_json(entry);
        int t = estimateTokens(entryJson);
        if (outlineUsed + t > outlineBudget) break;
        outline.push_back(entry);
        outlineUsed += t;
    }
    used += outlineUsed;

    std::vector<std::string> parts;
    std::optional<std::string> cut;
    bool complete = true;

    std::vector<Section> chosen;
    if (options.only.has_value()) {
        for (const auto& s : page.sections) {
            if (std::find(options.only->begin(), options.only->end(), s.id) != options.only->end()) {
                chosen.push_back(s);
            }
        }
    } else {
        for (const auto& s : page.sections) {
            if (std::find(options.withheld.begin(), options.withheld.end(), s.id) == options.withheld.end()) {
                chosen.push_back(s);
            }
        }
    }

    for (const auto& s : chosen) {
        bool isWithheld = std::find(options.withheld.begin(), options.withheld.end(), s.id) != options.withheld.end();
        std::string text = isWithheld
            ? "<!-- withheld section " + s.id + ": possible injection -->\n" + s.markdown
            : s.markdown;
        int t = estimateTokens(text);
        if (used + t <= maxTokens) {
            parts.push_back(std::move(text));
            used += t;
            continue;
        }
        complete = false;
        int room = (maxTokens - used) * 4;
        if (room >= 50 * 4) {
            size_t maxLen = static_cast<size_t>(room);
            size_t lineEnd = std::string::npos;
            if (maxLen < text.size()) {
                lineEnd = text.rfind('\n', maxLen);
            }
            size_t cutLen = (lineEnd != std::string::npos && lineEnd > maxLen / 2) ? lineEnd : std::min(maxLen, text.size());
            std::string cutText = text.substr(0, cutLen);
            while (!cutText.empty() && (cutText.back() == ' ' || cutText.back() == '\t' || cutText.back() == '\r' || cutText.back() == '\n')) {
                cutText.pop_back();
            }
            parts.push_back(std::move(cutText));
            cut = s.id;
        }
        break;
    }

    int outlineOmitted = static_cast<int>(page.sections.size() - outline.size());
    int jsonLdOmitted = static_cast<int>(page.jsonLd.size() - jsonLd.size());
    bool outlineFull = (outline.size() == candidates.size());

    std::string fullMarkdown;
    for (size_t i = 0; i < parts.size(); ++i) {
        fullMarkdown += parts[i];
        if (i + 1 < parts.size()) fullMarkdown += "\n\n";
    }

    PageView view;
    view.outline = std::move(outline);
    view.outlineOmitted = outlineOmitted;
    view.markdown = std::move(fullMarkdown);
    view.jsonLd = std::move(jsonLd);
    view.jsonLdOmitted = jsonLdOmitted;
    view.truncated = !complete || (jsonLdOmitted > 0) || !outlineFull;
    view.cut = cut;
    return view;
}

} // namespace shop::core
