#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "argParser.hpp"
#include "cookieJar.hpp"
#include "headerParser.hpp"
#include "url.hpp"

/** String utility functions. */
namespace StringUtils
{

/** Convert a string to lowercase. */
std::string toLower(std::string_view string);

/** Trim leading and trailing spaces and tabs. */
std::string trim(std::string_view string);

/** Format a system error message using strerror(errno). */
std::string systemError(const std::string& context);

/** Remove trailing newlines, carriage returns, and spaces. */
std::string_view rstrip(std::string_view string) noexcept;

/** Convert a character sequence to a base-10 number in the given range. */
std::expected<long, std::string> parseIntArg(const char* arg, long min, long max);

/** Compare two strings case-insensitively without allocation. */
bool iequals(std::string_view string1, std::string_view string2);

} // namespace StringUtils

namespace DebugUtils
{

/** Format parsed configuration for debugging. */
std::string configPrint(const ArgumentParser::Config& cfg);

/** Format a parsed URL for debugging. */
std::string urlPrint(const Urls::Url& url);

} // namespace DebugUtils

namespace UrlUtils
{

/** Resolve an HTTP redirect relative to a base URL. */
std::string resolveRedirect(const Urls::Url& base, std::string_view location);

} // namespace UrlUtils

namespace RequestBuilder
{
    std::string build(
        const Urls::Url& url,
        const ArgumentParser::Config& cfg,
        const HeaderParser& parser,
        const CookieJar& cookieJar
    );
} // namespace RequestBuilder