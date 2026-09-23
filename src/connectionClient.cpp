#include "connectionClient.hpp"

namespace Connections
{

std::expected<ConnectionClient, ConnectionError> ConnectionClient::connect(
    std::string_view host,
    std::string_view port,
    bool useTls,
    uint32_t receiveTimeoutMs,
    AddressFamily family)
{
    if (useTls) // Create an tls encypted connection
    {
        auto tlsRes = detail::TlsConnection::connect(host, port, family);
        if (!tlsRes)
        {
            return std::unexpected(tlsRes.error());
        }
        auto timeoutRes = tlsRes->setReceiveTimeout(receiveTimeoutMs);
        if (!timeoutRes)
        {
            return std::unexpected(timeoutRes.error());
        }
        return ConnectionClient{std::move(*tlsRes)};
    }
    else // Default nor encrypted tcp connection
    {
        auto tcpRes = detail::TcpConnection::connect(host, port, family);
        if (!tcpRes)
        {
            return std::unexpected(tcpRes.error());
        }
        auto timeoutRes = tcpRes->setReceiveTimeout(receiveTimeoutMs);
        if (!timeoutRes)
        {
            return std::unexpected(timeoutRes.error());
        }
        return ConnectionClient{std::move(*tcpRes)};
    }
}

std::expected<void, SendError> ConnectionClient::send(std::span<const std::byte> data)
{
    return std::visit([data](auto& conn) { return conn.send(data); }, connection_);
}

std::expected<size_t, ReceiveError> ConnectionClient::receive(std::span<std::byte> buffer)
{
    return std::visit([buffer](auto& conn) { return conn.receive(buffer); }, connection_);
}

} // namespace Connections