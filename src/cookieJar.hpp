#pragma once

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "headerParser.hpp"

/** Error returned when a Set-Cookie header cannot be parsed. */
struct CookieParseError
{
    std::optional<std::string> msg{};

    constexpr CookieParseError(std::string errorMessage = {}) noexcept
        : msg(errorMessage.empty() ? std::nullopt : std::optional{std::move(errorMessage)})
    {
    }
};

/** Stores cookies received from a server and generates Cookie header values. */
class CookieJar {
public:

    CookieJar() = default;

    /** Add a cookie received in a Set-Cookie header, replacing the same name. */
    void add(std::string_view setCookie);

    /** Extract and store all cookies from parsed response headers. */
    void update(const Response& response);

    /** Return true if no cookies are currently stored. */
    bool empty() const noexcept;

    /** Remove all stored cookies. */
    void clear();

    /** Serialize the stored cookies, returning an empty string when none exist. */
    std::string serialize() const;

private:

    /** Parse a name-value pair from a Set-Cookie header. */
    static std::expected<std::pair<std::string,std::string>, CookieParseError> parseCookie(std::string_view setCookie);

    /** Stored cookies indexed by name. */
    std::unordered_map<std::string,std::string> cookies_;
};