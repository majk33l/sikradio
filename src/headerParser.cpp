#include "headerParser.hpp"
#include "utils.hpp"

#include <cctype>
#include <charconv>
#include <vector>

static constexpr std::string_view HEADER_END = "\r\n\r\n";
static constexpr size_t MAX_HEADER_SIZE = 65536;

// Response methods

// Check if status code is a redirect
bool Response::isRedirect() const noexcept {
    return status == 301 || status == 302 || status == 303 ||
        status == 307 || status == 308;
}

// Check if status code is success
bool Response::isSuccess() const noexcept {
    return status == 200;
}

// Case insensitive header lookup
bool Response::hasHeader(std::string_view name) const {
    return headers.find(StringUtils::toLower(name)) != headers.end();
}

// Get all values for a header
std::vector<std::string> Response::headerAll(std::string_view name) const {
    auto [begin, end] = headers.equal_range(StringUtils::toLower(name));

    std::vector<std::string> result;
    result.reserve(static_cast<size_t>(std::distance(begin, end)));

    for (auto it = begin; it != end; ++it)
        result.push_back(it->second);

    return result;
}

// Check if response contains ICY metadata int
bool Response::hasMetaInt() const {
    return metaInt().has_value();
}

// Get ICY metadata int value
std::expected<size_t, HeaderParseError> Response::metaInt() const {
    auto it = headers.find("icy-metaint");

    if (it == headers.end())
        return std::unexpected(HeaderParseError("missing icy-metaint"));

    size_t value = 0;

    auto [ptr, ec] =
        std::from_chars(
            it->second.data(),
            it->second.data() + it->second.size(),
            value
        );

    if (ec != std::errc{} ||
        ptr != it->second.data() + it->second.size() ||
        value == 0) {
        return std::unexpected(HeaderParseError("invalid icy-metaint"));
    }

    return value;
}

// HeaderParser methods

// Reset parser
void HeaderParser::reset() {
    buffer_.clear();
    headerEndPos_ = std::string::npos;
    tooLarge_ = false;
}

// Append data chunk to receive buffer
// Searches for header end marker (\r\n\r\n)
void HeaderParser::append(const void* data, size_t size) {
    const char* bytes = static_cast<const char*>(data);

    buffer_.append(bytes, size);

    if (headerEndPos_ != std::string::npos)
        return;

    // Optimize search: only check the newly added portion
    size_t searchFrom = 0;

    if (buffer_.size() > size + 3)
        searchFrom = buffer_.size() - size - 3;

    auto pos = buffer_.find(HEADER_END, searchFrom);

    if (pos != std::string::npos)
    {
        headerEndPos_ = pos;
        tooLarge_ = pos > MAX_HEADER_SIZE;
    }
    else if (buffer_.size() > MAX_HEADER_SIZE)
    {
        tooLarge_ = true;
    }
}

bool HeaderParser::tooLarge() const noexcept {
    return tooLarge_;
}

// Check if complete header block has been received
bool HeaderParser::complete() const {
    return headerEndPos_ != std::string::npos;
}

// Get remaining data after header
std::string_view HeaderParser::remainingData() const {
    if (!complete())
        return {};

    return std::string_view(buffer_).substr(headerEndPos_ + HEADER_END.size());
}

// Get raw header block
std::string_view HeaderParser::rawHeaders() const {
    if (!complete())
        return {};

    std::string_view data(buffer_);

    return data.substr(0, headerEndPos_ + HEADER_END.size());
}

// Parse accumulated header data
std::expected<Response, HeaderParseError> HeaderParser::parse() const {
    if (!complete())
        return std::unexpected(HeaderParseError("headers incomplete"));

    return parseHeaders(std::string_view(buffer_).substr(0, headerEndPos_));
}

// Parse response headers from raw data
std::expected<Response, HeaderParseError> HeaderParser::parseHeaders(std::string_view raw) {
    Response resp;

    bool first = true;

    size_t pos = 0;

    while (pos < raw.size()) {
        size_t end = raw.find("\r\n", pos);
        if (end == std::string_view::npos)
            end = raw.size();

        auto line = raw.substr(pos, end - pos);

        if (first) {
            first = false;

            if (line.empty())
                return std::unexpected(HeaderParseError("empty status"));

            // Parse status line
            resp.statusLine = std::string(line);
            resp.icy = isIcyResponse(line);

            auto status = parseStatusCode(line);
            if (!status)
                return std::unexpected(status.error());

            resp.status = *status;
        } else {
            // Parse header line
            auto colon = line.find(':');

            if (colon != std::string_view::npos) {
                auto key = StringUtils::toLower(StringUtils::trim(line.substr(0, colon)));

                auto value = StringUtils::trim(line.substr(colon + 1));

                resp.headers.emplace(std::move(key), std::move(value));
            }
        }

        pos = end + 2;
    }

    return resp;
}

// Extract status code from status line
std::expected<int, HeaderParseError> HeaderParser::parseStatusCode(std::string_view line) {
    // Find first space to skip protocol
    auto sp = line.find(' ');

    if (sp == std::string_view::npos)
        return std::unexpected(HeaderParseError("invalid status line"));

    int code = 0;

    auto rest = line.substr(sp + 1);

    // Parse numeric status code
    auto [ptr, ec] =
        std::from_chars(
            rest.data(),
            rest.data() + rest.size(),
            code
        );

    if (ec != std::errc{})
        return std::unexpected(HeaderParseError("invalid status"));

    return code;
}

// Check if response is ICY format
bool HeaderParser::isIcyResponse(std::string_view line) {
    constexpr std::string_view prefix = "ICY ";

    return line.size() >= prefix.size() &&
           line.substr(0, prefix.size()) == prefix;
}
