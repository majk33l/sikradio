#pragma once

#include <optional>
#include <string>

namespace Connections
{

/** Errors that can occur while establishing a connection. */
enum class ConnectionErrorCode
{
    RESOLVE_FAILED,
    SOCKET_FAILED,
    CONNECT_FAILED,
    TLS_HANDSHAKE_FAILED,
};

/** Errors that can occur while sending data. */
enum class SendErrorCode
{
    SEND_FAILED,
    CONNECTION_CLOSED,
};

/** Errors that can occur while receiving data. */
enum class ReceiveErrorCode
{
    TIMEOUT,
    RECEIVE_FAILED,
    CONNECTION_CLOSED,
};


/** Error code and optional details for a connection failure. */

struct ConnectionError
{
    ConnectionErrorCode code;
    std::optional<std::string> msg{};

    constexpr ConnectionError(ConnectionErrorCode errorCode, std::string errorMessage = {}) noexcept
        : code(errorCode), msg(errorMessage.empty() ? std::nullopt : std::optional{std::move(errorMessage)})
    {
    }
};

/** Error code and optional details for a send failure. */
struct SendError
{
    SendErrorCode code;
    std::optional<std::string> msg{};

    constexpr SendError(SendErrorCode errorCode, std::string errorMessage = {}) noexcept
        : code(errorCode), msg(errorMessage.empty() ? std::nullopt : std::optional{std::move(errorMessage)})
    {
    }
};

/** Error code and optional details for a receive failure. */
struct ReceiveError
{
    ReceiveErrorCode code;
    std::optional<std::string> msg{};

    constexpr ReceiveError(ReceiveErrorCode errorCode, std::string errorMessage = {}) noexcept
        : code(errorCode), msg(errorMessage.empty() ? std::nullopt : std::optional{std::move(errorMessage)})
    {
    }
};

} // namespace Connections