#pragma once

#include <cstddef>
#include <expected>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct HeaderParseError
{
    std::optional<std::string> msg{};

    constexpr HeaderParseError(std::string errorMessage = {}) noexcept
        : msg(errorMessage.empty() ? std::nullopt : std::optional{std::move(errorMessage)})
    {
    }
};


/** Parsed response headers with normalized, lowercase names. */
struct Response {

    int status = 0;
    bool icy = false;

    /** Original status line. */
    std::string statusLine;

    /** Response headers keyed by lowercase names. */
     std::multimap<std::string, std::string> headers;

    bool isRedirect() const noexcept;
    bool isSuccess() const noexcept;

    /** Check for a header using a case-insensitive lookup. */
    bool hasHeader(std::string_view name) const;

    /** Return all values for a header, or an empty vector if it is absent. */
     std::vector<std::string> headerAll(std::string_view name) const;

    bool hasMetaInt() const;

    /** Return the icy-metaint value, or a HeaderParseError if invalid. */
    std::expected<size_t, HeaderParseError> metaInt() const;
};

/** Incrementally parses HTTP response headers. */
class HeaderParser {
public:

    HeaderParser() = default;

    void reset();
    void append(const void* data, size_t size);
    bool tooLarge() const noexcept;

    /** Return true once a complete header block has been received. */
    bool complete() const;

    /** Parse the accumulated data, or return a HeaderParseError. */
    std::expected<Response, HeaderParseError> parse() const;

    /** Return the bytes after the header terminator. */
    std::string_view remainingData() const;

    /** Return the raw bytes through the header terminator. */
    std::string_view rawHeaders() const;

private:
    std::string buffer_;

    /** Position of the header terminator, or std::npos until complete. */
    size_t headerEndPos_ = std::string::npos;
    bool tooLarge_ = false;

    /** Parse a complete raw header block. */
    static std::expected<Response, HeaderParseError> parseHeaders(std::string_view raw);
    static std::expected<int, HeaderParseError> parseStatusCode(std::string_view line);
    static bool isIcyResponse(std::string_view line);
};
