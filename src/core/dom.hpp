#pragma once

#include <lexbor/html/html.h>
#include <lexbor/html/serialize.h>
#include <lexbor/css/css.h>
#include <lexbor/selectors/selectors.h>

#include "json.hpp"

#include <cctype>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace shop::core {

class Node;

struct DocumentState {
    lxb_html_document_t* doc = nullptr;
    lxb_css_parser_t* parser = nullptr;
    lxb_selectors_t* selectors = nullptr;

    DocumentState() = default;
    ~DocumentState() {
        if (selectors) {
            lxb_selectors_destroy(selectors, true);
            selectors = nullptr;
        }
        if (parser) {
            // Lexbor's parser does not own its CSS selector arena.
            parser->memory = lxb_css_memory_destroy(parser->memory, true);
            lxb_css_parser_destroy(parser, true);
            parser = nullptr;
        }
        if (doc) {
            lxb_html_document_destroy(doc);
            doc = nullptr;
        }
    }

    DocumentState(const DocumentState&) = delete;
    DocumentState& operator=(const DocumentState&) = delete;

    void initSelectors() {
        if (!parser) {
            parser = lxb_css_parser_create();
            lxb_css_parser_init(parser, nullptr);
        }
        if (!selectors) {
            selectors = lxb_selectors_create();
            lxb_selectors_init(selectors);
            lxb_selectors_opt_set(selectors, LXB_SELECTORS_OPT_MATCH_FIRST);
        }
    }
};

class Node {
public:
    Node() = default;
    Node(lxb_dom_node_t* raw, std::shared_ptr<DocumentState> state)
        : raw_(raw), state_(std::move(state)) {}

    lxb_dom_node_t* raw() const noexcept { return raw_; }
    const std::shared_ptr<DocumentState>& state() const noexcept { return state_; }

    explicit operator bool() const noexcept { return raw_ != nullptr; }
    bool operator==(const Node& other) const noexcept { return raw_ == other.raw_; }
    bool operator!=(const Node& other) const noexcept { return raw_ != other.raw_; }

    std::string text() const {
        if (!raw_) return "";
        size_t len = 0;
        lxb_char_t* txt = lxb_dom_node_text_content(raw_, &len);
        if (!txt || len == 0) return "";
        return std::string(reinterpret_cast<const char*>(txt), len);
    }

    std::optional<std::string> attr(std::string_view name) const {
        if (!raw_ || raw_->type != LXB_DOM_NODE_TYPE_ELEMENT) return std::nullopt;
        lxb_dom_element_t* el = lxb_dom_interface_element(raw_);
        size_t val_len = 0;
        const lxb_char_t* val = lxb_dom_element_get_attribute(
            el, reinterpret_cast<const lxb_char_t*>(name.data()), name.size(), &val_len);
        if (!val) return std::nullopt;
        return std::string(reinterpret_cast<const char*>(val), val_len);
    }

    bool hasAttr(std::string_view name) const {
        if (!raw_ || raw_->type != LXB_DOM_NODE_TYPE_ELEMENT) return false;
        lxb_dom_element_t* el = lxb_dom_interface_element(raw_);
        return lxb_dom_element_has_attribute(
            el, reinterpret_cast<const lxb_char_t*>(name.data()), name.size());
    }

    void setAttr(std::string_view name, std::string_view val) const {
        if (!raw_ || raw_->type != LXB_DOM_NODE_TYPE_ELEMENT) return;
        lxb_dom_element_t* el = lxb_dom_interface_element(raw_);
        lxb_dom_element_set_attribute(
            el,
            reinterpret_cast<const lxb_char_t*>(name.data()), name.size(),
            reinterpret_cast<const lxb_char_t*>(val.data()), val.size());
    }

    void removeAttr(std::string_view name) const {
        if (!raw_ || raw_->type != LXB_DOM_NODE_TYPE_ELEMENT) return;
        lxb_dom_element_t* el = lxb_dom_interface_element(raw_);
        lxb_dom_element_remove_attribute(
            el, reinterpret_cast<const lxb_char_t*>(name.data()), name.size());
    }

    void remove() const {
        if (raw_) {
            lxb_dom_node_remove(raw_);
        }
    }

    std::string innerHtml() const {
        if (!raw_) return "";
        std::string out;
        auto cb = [](const lxb_char_t* data, size_t len, void* ctx) -> lxb_status_t {
            static_cast<std::string*>(ctx)->append(reinterpret_cast<const char*>(data), len);
            return LXB_STATUS_OK;
        };
        lxb_html_serialize_deep_cb(raw_, +cb, &out);
        return out;
    }

    std::string outerHtml() const {
        if (!raw_) return "";
        std::string out;
        auto cb = [](const lxb_char_t* data, size_t len, void* ctx) -> lxb_status_t {
            static_cast<std::string*>(ctx)->append(reinterpret_cast<const char*>(data), len);
            return LXB_STATUS_OK;
        };
        lxb_html_serialize_tree_cb(raw_, +cb, &out);
        return out;
    }

    std::string tagName() const {
        if (!raw_ || raw_->type != LXB_DOM_NODE_TYPE_ELEMENT) return "";
        lxb_dom_element_t* el = lxb_dom_interface_element(raw_);
        size_t len = 0;
        const lxb_char_t* name = lxb_dom_element_local_name(el, &len);
        if (!name) return "";
        std::string s(reinterpret_cast<const char*>(name), len);
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    std::string nodeName() const {
        if (!raw_) return "";
        if (raw_->type == LXB_DOM_NODE_TYPE_ELEMENT) {
            lxb_dom_element_t* el = lxb_dom_interface_element(raw_);
            size_t len = 0;
            const lxb_char_t* name = lxb_dom_element_qualified_name_upper(el, &len);
            if (name) return std::string(reinterpret_cast<const char*>(name), len);
        } else if (raw_->type == LXB_DOM_NODE_TYPE_TEXT) {
            return "#text";
        } else if (raw_->type == LXB_DOM_NODE_TYPE_COMMENT) {
            return "#comment";
        } else if (raw_->type == LXB_DOM_NODE_TYPE_DOCUMENT) {
            return "#document";
        }
        return "";
    }

    std::optional<Node> parent() const {
        if (!raw_ || !raw_->parent) return std::nullopt;
        return Node(raw_->parent, state_);
    }

    std::optional<Node> nextSibling() const {
        if (!raw_ || !raw_->next) return std::nullopt;
        return Node(raw_->next, state_);
    }

    std::optional<Node> prevSibling() const {
        if (!raw_ || !raw_->prev) return std::nullopt;
        return Node(raw_->prev, state_);
    }

    std::vector<Node> children() const {
        std::vector<Node> result;
        if (!raw_) return result;
        lxb_dom_node_t* child = raw_->first_child;
        while (child) {
            if (child->type == LXB_DOM_NODE_TYPE_ELEMENT) {
                result.emplace_back(child, state_);
            }
            child = child->next;
        }
        return result;
    }

    std::vector<Node> childNodes() const {
        std::vector<Node> result;
        if (!raw_) return result;
        lxb_dom_node_t* child = raw_->first_child;
        while (child) {
            result.emplace_back(child, state_);
            child = child->next;
        }
        return result;
    }

    bool isConnected() const {
        if (!raw_) return false;
        lxb_dom_node_t* cur = raw_;
        while (cur->parent) cur = cur->parent;
        return cur->type == LXB_DOM_NODE_TYPE_DOCUMENT;
    }

    bool contains(const Node& other) const {
        if (!raw_ || !other.raw_) return false;
        lxb_dom_node_t* cur = other.raw_;
        while (cur) {
            if (cur == raw_) return true;
            cur = cur->parent;
        }
        return false;
    }

    std::optional<Node> querySelector(std::string_view selector) const {
        if (!raw_ || !state_) return std::nullopt;
        state_->initSelectors();
        lxb_css_selector_list_t* list = lxb_css_selectors_parse(
            state_->parser,
            reinterpret_cast<const lxb_char_t*>(selector.data()),
            selector.size());
        if (!list || state_->parser->status != LXB_STATUS_OK) {
            lxb_css_parser_erase(state_->parser);
            return std::nullopt;
        }
        struct Context {
            lxb_dom_node_t* match = nullptr;
        } ctx;
        auto cb = [](lxb_dom_node_t* node, lxb_css_selector_specificity_t, void* ptr) -> lxb_status_t {
            static_cast<Context*>(ptr)->match = node;
            return LXB_STATUS_STOP;
        };
        lxb_selectors_find(state_->selectors, raw_, list, +cb, &ctx);
        lxb_css_parser_erase(state_->parser);
        if (ctx.match) return Node(ctx.match, state_);
        return std::nullopt;
    }

    std::vector<Node> querySelectorAll(std::string_view selector) const {
        std::vector<Node> results;
        if (!raw_ || !state_) return results;
        state_->initSelectors();
        lxb_css_selector_list_t* list = lxb_css_selectors_parse(
            state_->parser,
            reinterpret_cast<const lxb_char_t*>(selector.data()),
            selector.size());
        if (!list || state_->parser->status != LXB_STATUS_OK) {
            lxb_css_parser_erase(state_->parser);
            return results;
        }
        struct Context {
            std::vector<Node>* res;
            std::shared_ptr<DocumentState> state;
        } ctx{&results, state_};
        auto cb = [](lxb_dom_node_t* node, lxb_css_selector_specificity_t, void* ptr) -> lxb_status_t {
            auto* c = static_cast<Context*>(ptr);
            c->res->emplace_back(node, c->state);
            return LXB_STATUS_OK;
        };
        lxb_selectors_find(state_->selectors, raw_, list, +cb, &ctx);
        lxb_css_parser_erase(state_->parser);
        return results;
    }

private:
    lxb_dom_node_t* raw_ = nullptr;
    std::shared_ptr<DocumentState> state_;
};

class Document {
public:
    Document() = default;
    explicit Document(std::shared_ptr<DocumentState> state) : state_(std::move(state)) {}

    static Document parse(std::string_view html) {
        auto state = std::make_shared<DocumentState>();
        state->doc = lxb_html_document_create();
        lxb_html_document_parse(
            state->doc,
            reinterpret_cast<const lxb_char_t*>(html.data()),
            html.size());
        return Document(std::move(state));
    }

    Node rootNode() const {
        if (!state_ || !state_->doc) return Node();
        return Node(lxb_dom_interface_node(state_->doc), state_);
    }

    Node documentElement() const {
        if (!state_ || !state_->doc) return Node();
        lxb_dom_element_t* el = lxb_dom_document_element(lxb_dom_interface_document(state_->doc));
        return Node(lxb_dom_interface_node(el), state_);
    }

    Node body() const {
        if (!state_ || !state_->doc) return Node();
        lxb_html_body_element_t* b = lxb_html_document_body_element(state_->doc);
        return Node(lxb_dom_interface_node(b), state_);
    }

    std::string title() const {
        auto t = querySelector("title");
        if (t) return t->text();
        return "";
    }

    std::optional<Node> querySelector(std::string_view selector) const {
        return rootNode().querySelector(selector);
    }

    std::vector<Node> querySelectorAll(std::string_view selector) const {
        return rootNode().querySelectorAll(selector);
    }

    std::vector<slimr::Json> jsonLd() const {
        std::vector<slimr::Json> blocks;
        auto scripts = querySelectorAll("script[type=\"application/ld+json\"]");
        for (const auto& s : scripts) {
            std::string txt = s.text();
            auto parsed = slimr::parse_json<slimr::Json>(txt);
            if (parsed) {
                blocks.push_back(std::move(*parsed));
            }
        }
        return blocks;
    }

    void removeComments() const {
        auto walk = [](auto& self, lxb_dom_node_t* n) -> void {
            lxb_dom_node_t* child = n->first_child;
            while (child) {
                lxb_dom_node_t* next = child->next;
                if (child->type == LXB_DOM_NODE_TYPE_COMMENT) {
                    lxb_dom_node_remove(child);
                } else if (child->type == LXB_DOM_NODE_TYPE_ELEMENT) {
                    self(self, child);
                }
                child = next;
            }
        };
        if (state_ && state_->doc) {
            walk(walk, lxb_dom_interface_node(state_->doc));
        }
    }

private:
    std::shared_ptr<DocumentState> state_;
};

inline std::vector<slimr::Json> jsonLdBlocks(std::string_view html) {
    auto doc = Document::parse(html);
    return doc.jsonLd();
}

inline std::optional<slimr::Json> findLd(const std::vector<slimr::Json>& blocks, std::string_view type) {
    std::deque<slimr::Json> queue(blocks.begin(), blocks.end());
    while (!queue.empty()) {
        slimr::Json node = std::move(queue.front());
        queue.pop_front();
        if (node.is_array()) {
            for (auto& item : node.get<slimr::Json::array_t>()) {
                queue.push_back(std::move(item));
            }
        } else if (node.is_object()) {
            auto& obj = node.get<slimr::Json::object_t>();
            auto it = obj.find("@type");
            if (it != obj.end()) {
                if (it->second.is_string() && it->second.get<std::string>() == type) {
                    return node;
                } else if (it->second.is_array()) {
                    for (const auto& elem : it->second.get<slimr::Json::array_t>()) {
                        if (elem.is_string() && elem.get<std::string>() == type) {
                            return node;
                        }
                    }
                }
            }
            auto git = obj.find("@graph");
            if (git != obj.end()) {
                queue.push_back(git->second);
            }
        }
    }
    return std::nullopt;
}

inline std::optional<slimr::Json> findLd(const slimr::Json& root, std::string_view type) {
    return findLd(std::vector<slimr::Json>{root}, type);
}

} // namespace shop::core
