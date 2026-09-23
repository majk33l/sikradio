#include "url.hpp"
#include "utils.hpp"

#include <cctype>
#include <charconv>
#include <utility>

namespace {

constexpr std::string_view SCHEME_HTTP  = "http";
constexpr std::string_view SCHEME_HTTPS = "https";

constexpr std::string_view DEFAULT_PORT_HTTP  = "80";
constexpr std::string_view DEFAULT_PORT_HTTPS = "443";

constexpr std::string_view SCHEME_SEPARATOR = "://";


// Extract and validate scheme from URL.
std::expected<std::pair<Urls::Scheme, std::string_view>, std::string>
splitScheme(std::string_view url)
{
    auto pos = url.find(SCHEME_SEPARATOR);
    if (pos == std::string_view::npos)
        return std::pair(Urls::Scheme::HTTP, url);

    const std::string_view raw_scheme = url.substr(0, pos);

    if (!StringUtils::iequals(raw_scheme, SCHEME_HTTP) &&
            !StringUtils::iequals(raw_scheme, SCHEME_HTTPS))
        return std::unexpected(
            "Unsupported scheme '" +
            std::string(raw_scheme) +
            "'. Only http and https are accepted."
        );

    const Urls::Scheme scheme = StringUtils::iequals(raw_scheme, SCHEME_HTTP)
        ? Urls::Scheme::HTTP
        : Urls::Scheme::HTTPS;

    const std::string_view rest = url.substr(pos + SCHEME_SEPARATOR.size());
    if (rest.empty())
        return std::unexpected(
        "Missing host in URL: " +
            std::string(url)
        );

    return std::pair(scheme, rest);
}

// Split host and port from path.
std::pair<std::string_view, std::string_view>
splitAuthorityAndPath(std::string_view rest)
{
    auto slashPos = rest.find('/');

    if (slashPos == std::string_view::npos)
        return {rest, "/"};

    return {rest.substr(0, slashPos), rest.substr(slashPos)};
}

// Split host and port from authority.
std::expected<std::pair<std::string_view, std::string_view>, std::string>
splitHostAndPort(std::string_view authority)
{
    if (authority.empty())
        return std::unexpected("Empty host.");

    // IPv6 literal format: [::1]:port or [::1]
    if (authority.front() == '[')
    {
        auto closeBracket = authority.find(']');
        if (closeBracket == std::string_view::npos)
            return std::unexpected(
                "Malformed IPv6 literal (missing ']'): " +
                std::string(authority)
            );

        const std::string_view host = authority.substr(0, closeBracket + 1);
        const std::string_view rest = authority.substr(closeBracket + 1);

        if (rest.empty())
            return std::pair(host, std::string_view());

        if (rest.front() != ':')
            return std::unexpected(
                "Unexpected character after IPv6 literal: " +
                std::string(authority)
            );

        return std::pair(host, rest.substr(1));
    }

    // Regular hostname:port format.
    auto colonPos = authority.find(':');
    if (colonPos == std::string_view::npos)
        return std::pair(authority, std::string_view());

    return std::pair(
        authority.substr(0, colonPos),
        authority.substr(colonPos + 1)
    );
}

// Validate port number
std::expected<void, std::string> validatePort(std::string_view port)
{
    if (port.empty())
        return {};

    int val{};
    const auto [ptr, ec] = std::from_chars(port.data(), port.data() + port.size(), val);

    if (ec != std::errc{} || ptr != port.data() + port.size())
        return std::unexpected(
            "Invalid port (non-numeric): " +
            std::string(port)
        );

    if (val < 1 || val > 65535)
        return std::unexpected(
            "Port out of range [1, 65535]: " +
            std::string(port)
        );

    return {};
}

// Get default port for scheme.
std::expected<std::string_view, std::string> defaultPort(Urls::Scheme scheme)
{
    switch (scheme)
    {
        case Urls::Scheme::HTTP:  return DEFAULT_PORT_HTTP;
        case Urls::Scheme::HTTPS: return DEFAULT_PORT_HTTPS;
        default:                  return std::unexpected("Url scheme resolving error");
    }
}

} // namespace

namespace Urls
{

std::expected<Url, std::string> parse(const std::string& raw_url)
{
    auto schemeResult = splitScheme(raw_url);
    if (!schemeResult)
        return std::unexpected(std::move(schemeResult.error()));

    const auto [scheme, rest] = schemeResult.value();

    const auto [authority, path] = splitAuthorityAndPath(rest);

    auto hostPortResult = splitHostAndPort(authority);
    if (!hostPortResult)
        return std::unexpected(std::move(hostPortResult.error()));

    const auto [host, port] = hostPortResult.value();

    if (auto portCheck = validatePort(port); !portCheck)
        return std::unexpected(std::move(portCheck.error()));

    if (host.empty())
        return std::unexpected("Empty host in URL: " + raw_url);

    std::string finalPort;
    if (port.empty())
    {
        auto def = defaultPort(scheme);
        if (!def)
            return std::unexpected(std::move(def.error()));
        finalPort = def.value();
    }
    else
    {
        finalPort = port;
    }

    Url result;
    result.scheme = scheme;
    result.host   = std::string(host);
    result.port   = std::move(finalPort);
    result.path   = std::string(path);

    return result;
}

}