#include "utils.hpp"

#include "headerParser.hpp"
#include "cookieJar.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cstring>
#include <sstream>
#include <vector>

namespace StringUtils
{

std::string toLower(std::string_view string) {
    std::string out(string);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return out;
}

std::string trim(std::string_view string) {
    constexpr std::string_view wstring = " \t";

    auto start = string.find_first_not_of(wstring);

    if (start == std::string_view::npos)
        return {};

    auto end = string.find_last_not_of(wstring);

    return std::string(string.substr(start, end - start + 1));
}

std::string systemError(const std::string& context) {
    return context + ": " + std::strerror(errno);
}

std::string_view rstrip(std::string_view string) noexcept
{
    size_t end = string.size();
    while (end > 0 && (string[end - 1] == '\n' || string[end - 1] == '\r' || string[end - 1] == ' '))
        --end;

    return std::string_view(string.data(), end);
}

std::expected<long, std::string>
parseIntArg(const char* arg, long min, long max)
{
    long value;

    auto [ptr, ec] = std::from_chars(arg, arg + std::strlen(arg), value);

    if (ec != std::errc{} || ptr != arg + std::strlen(arg))
        return std::unexpected("Invalid integer value: " + std::string(arg));

    if (value < min || value > max)
        return std::unexpected(
            "Value " + std::to_string(value) +
            " is outside allowed range [" +
            std::to_string(min) + ", " +
            std::to_string(max) + "]"
        );

    return value;
}

bool iequals(std::string_view string1, std::string_view string2)
{
    return std::ranges::equal(string1, string2, [](unsigned char c1, unsigned char c2)
    {
        return std::tolower(c1) == std::tolower(c2);
    });
}

} // namespace StringUtils

namespace DebugUtils
{

std::string configPrint(const ArgumentParser::Config& cfg) {
    std::string result;

    auto addressFamilyStr = [](Connections::AddressFamily family) -> std::string
    {
        switch (family)
        {
            case Connections::AddressFamily::IPv6:
                return "IPv6 Only";
            case Connections::AddressFamily::IPv4:
                return "IPv4 Only";
            default:
                return "Auto";
        }
    };

    result += "Parsed config:\n";
    result += "URL: " + cfg.url + "\n";
    result += "Multiplex: " + std::string(cfg.multiplex ? "true" : "false") + "\n";
    result += "Timeout: " + std::to_string(cfg.timeoutMs) + " ms\n";
    result += "Verbosity: " + std::to_string(static_cast<int>(cfg.verbosity)) + "\n";
    result += "Address Family: " + addressFamilyStr(cfg.addressFamily) + "\n";
    result += "\n";

    return result;
}

std::string urlPrint(const Urls::Url& url) {
    std::string result;

    result += "Parsed URL:\n";
    result += "Scheme: " + std::string(url.scheme == Urls::Scheme::HTTPS ? "https" : "http") + "\n";
    result += "Host: " + url.host + "\n";
    result += "Port: " + url.port + "\n";
    result += "Path: " + url.path + "\n";
    return result;
}

} // namespace DebugUtils

namespace UrlUtils
{

namespace
{

bool startsWithIgnoreCase(std::string_view value, std::string_view prefix)
{
    return value.size() >= prefix.size() &&
        StringUtils::iequals(value.substr(0, prefix.size()), prefix);
}

std::string normalizePath(std::string_view path)
{
    std::vector<std::string_view> segments;
    size_t segmentStart = path.starts_with('/') ? 1 : 0;

    while (segmentStart <= path.size())
    {
        const size_t separator = path.find('/', segmentStart);
        const size_t segmentEnd = separator == std::string_view::npos
            ? path.size()
            : separator;
        const std::string_view segment = path.substr(
            segmentStart,
            segmentEnd - segmentStart);

        if (segment == "..")
        {
            if (!segments.empty())
                segments.pop_back();
        }
        else if (!segment.empty() && segment != ".")
        {
            segments.push_back(segment);
        }

        if (separator == std::string_view::npos)
            break;
        segmentStart = separator + 1;
    }

    std::string result = "/";
    for (size_t index = 0; index < segments.size(); ++index)
    {
        if (index > 0)
            result += '/';
        result += segments[index];
    }

    if (path.ends_with('/') && result.back() != '/')
        result += '/';

    return result;
}

} // namespace

std::string resolveRedirect(const Urls::Url& base, std::string_view location)
{
    if (startsWithIgnoreCase(location, "http://") ||
        startsWithIgnoreCase(location, "https://"))
    {
        return std::string(location);
    }

    const std::string scheme = base.scheme == Urls::Scheme::HTTPS
        ? "https://"
        : "http://";

    std::string origin = scheme + base.host;
    const bool defaultPort =
        (base.scheme == Urls::Scheme::HTTP && base.port == "80") ||
        (base.scheme == Urls::Scheme::HTTPS && base.port == "443");
    if (!defaultPort)
        origin += ':' + base.port;

    if (location.starts_with("//"))
        return (base.scheme == Urls::Scheme::HTTPS ? "https:" : "http:") +
            std::string(location);

    const size_t suffixStart = location.find_first_of("?#");
    const std::string_view locationPath = location.substr(0, suffixStart);
    const std::string_view suffix = suffixStart == std::string_view::npos
        ? std::string_view{}
        : location.substr(suffixStart);

    std::string path;
    if (locationPath.empty() && !suffix.empty())
    {
        path = base.path.substr(0, base.path.find_first_of("?#"));
    }
    else if (locationPath.starts_with('/'))
    {
        path = std::string(locationPath);
    }
    else
    {
        const size_t lastSlash = base.path.find_last_of('/');
        const std::string baseDirectory = lastSlash == std::string::npos
            ? "/"
            : base.path.substr(0, lastSlash + 1);
        path = baseDirectory + std::string(locationPath);
    }

    return origin + normalizePath(path) + std::string(suffix);
}

} // namespace UrlUtils

namespace RequestBuilder
{

std::string build(
    const Urls::Url& url,
    const ArgumentParser::Config& cfg,
    const HeaderParser&,
    const CookieJar& cookieJar
)
{
    std::ostringstream request;

    request << "GET "
            << url.path
            << " HTTP/1.1\r\n";

    request << "Host: ";
    if (url.host.find(':') != std::string::npos &&
        (url.host.empty() || url.host.front() != '['))
    {
        request << '[' << url.host << ']';
    }
    else
    {
        request << url.host;
    }

    const bool isDefaultPort =
        (url.scheme == Urls::Scheme::HTTP && url.port == "80") ||
        (url.scheme == Urls::Scheme::HTTPS && url.port == "443");

    if (!isDefaultPort)
    {
        request << ':' << url.port;
    }
    request << "\r\n";

    request << "Connection: keep-alive\r\n";

    if (cfg.multiplex)
    {
        request << "Icy-MetaData: 1\r\n";
    }

    if (!cookieJar.empty())
    {
        request << "Cookie: " << cookieJar.serialize() << "\r\n";
    }

    request << "\r\n";

    return request.str();
}

} // namespace RequestBuilder