#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <variant>

#include "addressFamily.hpp"
#include "connectionError.hpp"
#include "connectionTcp.hpp"
#include "connectionTls.hpp"

namespace Connections
{

/** Wrapper for TCP and TLS connections used by sikradio. */
class ConnectionClient
{
public:
    /** Create a connection, or return a ConnectionError on failure. */
    static std::expected<ConnectionClient, ConnectionError> connect(
        std::string_view host,
        std::string_view port,
        bool useTls,
        uint32_t receiveTimeoutMs,
        AddressFamily family = AddressFamily::ANY);

    ConnectionClient(ConnectionClient&&) noexcept = default;
    ConnectionClient& operator=(ConnectionClient&&) noexcept = default;
    ConnectionClient(const ConnectionClient&) = delete;
    ConnectionClient& operator=(const ConnectionClient&) = delete;
    ~ConnectionClient() = default;

    /** Send all bytes, or return a SendError on failure. */
    std::expected<void, SendError> send(std::span<const std::byte> data);

    /** Receive bytes, waiting for the configured timeout when no data is available. */
    std::expected<size_t, ReceiveError> receive(std::span<std::byte> buffer);

private:
    /** Construct a client from an established connection. */
    explicit ConnectionClient(std::variant<detail::TcpConnection, detail::TlsConnection> connection)
        : connection_(std::move(connection)) {}

    std::variant<detail::TcpConnection, detail::TlsConnection> connection_;
};

} // namespace Connections