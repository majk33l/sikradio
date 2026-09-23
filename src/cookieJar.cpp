#include "cookieJar.hpp"
#include "utils.hpp"

// Add a single cookie to the jar
// Parses name=value from Set-Cookie header
void CookieJar::add(std::string_view setCookie) {
    auto cookie = parseCookie(setCookie);

    // Ignore malformed cookies silently
    if (!cookie)
        return;

    auto& [name, value] = *cookie;

    // Store or replace existing cookie
    cookies_[std::move(name)] = std::move(value);
}

// Extract and store all cookies from response headers
void CookieJar::update(const Response& response) {
    for (const auto& cookie : response.headerAll("set-cookie"))
        add(cookie);
}

// Check if cookie jar is empty
bool CookieJar::empty() const noexcept {
    return cookies_.empty();
}

// Clear all stored cookies
void CookieJar::clear() {
    cookies_.clear();
}

// Serialize cookies for Cookie request header
std::string CookieJar::serialize() const {
    // Return empty string if no cookies
    if (empty())
        return "";

    // Reserve size for the buffer
    size_t size = 0;
    for (const auto& [name, value] : cookies_)
        size += name.size() + value.size() + 3; // '=' + "; "

    std::string out;
    out.reserve(size);

    bool first = true;

    // Join cookies with "; "
    for (const auto& [name, value] : cookies_) {
        if (!first)
            out += "; ";

        first = false;

        out += name;
        out += '=';
        out += value;
    }

    return out;
}

// Parse name=value from Set-Cookie header
// Extracts only the cookie value, ignoring attributes
std::expected<std::pair<std::string,std::string>, CookieParseError>
CookieJar::parseCookie(std::string_view setCookie) {
    size_t semicolon = setCookie.find(';');

    if (semicolon != std::string_view::npos)
        setCookie = setCookie.substr(0, semicolon);

    size_t equal = setCookie.find('=');

    if (equal == std::string_view::npos)
        return std::unexpected(CookieParseError("missing '='"));

    // Extract name and value
    std::string name = StringUtils::trim(setCookie.substr(0, equal));
    std::string value = StringUtils::trim(setCookie.substr(equal + 1));

    if (name.empty())
        return std::unexpected(CookieParseError("empty cookie name"));

    return std::pair{
        std::move(name),
        std::move(value)
    };
}
