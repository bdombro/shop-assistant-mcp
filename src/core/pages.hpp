#pragma once

#include "dom.hpp"
#include "policy.hpp"
#include "markdown.hpp"

#include <regex>
#include <string>
#include <string_view>

namespace shop::core {

inline bool isChallenge(std::string_view body) {
    std::string_view prefix = body.substr(0, std::min<size_t>(body.size(), 20000));
    static const std::regex challengeRe(
        R"(cf-challenge|challenge-platform|cf-turnstile|captcha-delivery\.com|validateCaptcha|Robot Check|unusual traffic|are you a robot|\/httpservice\/retry\/enablejs|google\.com\/sorry\/|bm-verify|<title>\s*Human Verification\s*<\/title>|confirm you are human)",
        std::regex::icase);
    return std::regex_search(prefix.begin(), prefix.end(), challengeRe);
}

const std::string SETTLE_STATE = R"((() => {
  const len = document.body ? document.body.innerText.length : 0;
  const wafPending =
    (Boolean(document.querySelector('script[src*="awswaf.com"]')) && !document.cookie.includes("aws-waf-token=")) ||
    document.title === "Just a moment..." ||
    Boolean(document.querySelector('script[src*="_sec/cp_challenge"]'));
  return { len, wafPending };
})())";

const std::string MARK_HIDDEN = R"(() => {
  for (const el of document.body ? document.body.querySelectorAll("*") : []) {
    const s = getComputedStyle(el);
    const r = el.getBoundingClientRect();
    const offscreen = r.right < -500 || r.bottom < -500;
    if (s.display === "none" || s.visibility === "hidden" || Number(s.opacity) === 0 || parseFloat(s.fontSize) === 0 || offscreen) {
      el.setAttribute("data-sa-hidden", "");
    }
  }
  const json = /json/.test(document.contentType) ? document.body.innerText : undefined;
  return { html: document.documentElement.outerHTML, href: location.href, json };
})";

} // namespace shop::core
