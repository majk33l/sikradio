#pragma once

#include <cstdint>
#include <expected>
#include <string>


namespace Urls
{

/** URL scheme. */
enum class Scheme : uint8_t { HTTP, HTTPS };

/** Parsed URL components. */
struct Url
{
    Scheme scheme;
    std::string host;
    std::string port;
    std::string path;

};

/** Parse a raw URL, or return an error string on failure. */
std::expected<Url, std::string> parse(const std::string& raw_url);

} // namespace Urls