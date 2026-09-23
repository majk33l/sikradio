#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

#include "addressFamily.hpp"
#include "connectionError.hpp"

/** Internal connection implementations. */
namespace Connections::detail
{

/** Wrapper for a TCP connection. */
class TcpConnection
{
public:
    /** Create a TCP connection, or return a ConnectionError on failure. */
    static std::expected<TcpConnection, ConnectionError> connect(
        std::string_view host,
        std::string_view port,
        AddressFamily family = AddressFamily::ANY);

    TcpConnection(TcpConnection&& other) noexcept;
    TcpConnection& operator=(TcpConnection&& other) noexcept;
    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;
    ~TcpConnection();

    /** Send all bytes, or return a SendError on failure. */
    std::expected<void, SendError> send(std::span<const std::byte> data);

    /** Receive bytes, waiting for the configured timeout when no data is available. */
    std::expected<size_t, ReceiveError> receive(std::span<std::byte> buffer);

    /** Set the timeout for receive operations. */
    std::expected<void, ConnectionError> setReceiveTimeout(uint32_t timeoutMs) noexcept;

    /** Return the socket file descriptor. */
    [[nodiscard]] int descriptor() const noexcept { return fd_; }

private:
    /** Construct a connection from a socket file descriptor. */
    explicit TcpConnection(int fd) noexcept : fd_(fd) {}

    /** Socket file descriptor. */
    int fd_ = -1;
};

} // namespace Connections::detail