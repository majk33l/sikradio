#include "connectionTcp.hpp"

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string>
#include <utility>

namespace Connections::detail
{

namespace
{

int toAiFamily(AddressFamily family) noexcept
{
    switch (family)
    {
    case AddressFamily::IPv4:
        return AF_INET;
    case AddressFamily::IPv6:
        return AF_INET6;
    case AddressFamily::ANY:
    default:
        return AF_UNSPEC;
    }
}

std::expected<void, ConnectionError> setReceiveTimeout(int fd, uint32_t timeoutMs) noexcept
{
    struct timeval tv{};
    tv.tv_sec = static_cast<time_t>(timeoutMs / 1000);
    tv.tv_usec = static_cast<suseconds_t>((timeoutMs % 1000) * 1000);

    if (::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        return std::unexpected(ConnectionError{ConnectionErrorCode::SOCKET_FAILED, ::strerror(errno)});
    }

    return {};
}

} // namespace

TcpConnection::TcpConnection(TcpConnection&& other) noexcept
    : fd_(std::exchange(other.fd_, -1))
{
}

TcpConnection& TcpConnection::operator=(TcpConnection&& other) noexcept
{
    if (this != &other)
    {
        if (fd_ != -1)
        {
            ::close(fd_);
        }
        fd_ = std::exchange(other.fd_, -1);
    }
    return *this;
}

TcpConnection::~TcpConnection()
{
    if (fd_ != -1)
    {
        ::close(fd_);
    }
}

std::expected<TcpConnection, ConnectionError> TcpConnection::connect(
    std::string_view host,
    std::string_view port,
    AddressFamily family)
{
    std::string hostStr{host};

    // Normalize ipv6 str fromat for getaddrinfo
    if (hostStr.size() >= 2 && hostStr.front() == '[' && hostStr.back() == ']')
    {
        hostStr = hostStr.substr(1, hostStr.size() - 2);
    }

    addrinfo hints{};
    hints.ai_family = toAiFamily(family);
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    int gaiRes = ::getaddrinfo(hostStr.c_str(), std::string(port).c_str(), &hints, &result);
    if (gaiRes != 0)
    {
        return std::unexpected(ConnectionError{ConnectionErrorCode::RESOLVE_FAILED, ::gai_strerror(gaiRes)});
    }

    ConnectionError lastError{ConnectionErrorCode::CONNECT_FAILED, "Failed to connect to host"};

    for (addrinfo* rp = result; rp != nullptr; rp = rp->ai_next)
    {
        int fd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0)
        {
            lastError = ConnectionError{ConnectionErrorCode::SOCKET_FAILED, ::strerror(errno)};
            continue;
        }

        // Try conneting to resolved addresses
        int connectResult = ::connect(fd, rp->ai_addr, rp->ai_addrlen);
        if (connectResult < 0)
        {
            lastError = ConnectionError{ConnectionErrorCode::CONNECT_FAILED, ::strerror(errno)};
            ::close(fd);
            continue;
        }

        ::freeaddrinfo(result);
        // Return made connecton on success
        return TcpConnection{fd};
    }

    ::freeaddrinfo(result);
    // None of the resolved addresss were successful
    return std::unexpected(lastError);
}

std::expected<void, ConnectionError> TcpConnection::setReceiveTimeout(uint32_t timeoutMs) noexcept
{
    return ::Connections::detail::setReceiveTimeout(fd_, timeoutMs);
}

std::expected<void, SendError> TcpConnection::send(std::span<const std::byte> data)
{
    size_t totalSent = 0;

    // Data sending loop - attempts to send the whole buffer
    while (totalSent < data.size())
    {
        ssize_t sent = ::send(
            fd_,
            reinterpret_cast<const char*>(data.data()) + totalSent,
            data.size() - totalSent,
            MSG_NOSIGNAL);

        if (sent < 0)
        {
            if (errno == EINTR)
            {
                // Interrupted by signal - conntinue the attempt
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return std::unexpected(SendError{SendErrorCode::SEND_FAILED, "Send socket would block"});
            }
            return std::unexpected(SendError{SendErrorCode::SEND_FAILED, ::strerror(errno)});
        }

        if (sent == 0)
        {
            return std::unexpected(SendError{SendErrorCode::CONNECTION_CLOSED, "Connection closed while sending"});
        }

        totalSent += static_cast<size_t>(sent);
    }

    return {};
}

std::expected<size_t, ReceiveError> TcpConnection::receive(std::span<std::byte> buffer)
{
    // Return 0 if the buffer is empty
    if (buffer.empty())
    {
        return 0;
    }

    // Trye receiving one portioin of bytes
    while (true)
    {
        ssize_t bytesRead = ::recv(fd_, buffer.data(), buffer.size(), 0);

        if (bytesRead < 0)
        {
            if (errno == EINTR)
            {
                // Interrupted by signal - try again
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // Data receiving timeout
                return std::unexpected(ReceiveError{ReceiveErrorCode::TIMEOUT, "Receive socket timeout exceeded"});
            }
            return std::unexpected(ReceiveError{ReceiveErrorCode::RECEIVE_FAILED, ::strerror(errno)});
        }

        if (bytesRead == 0)
        {
            return std::unexpected(ReceiveError{ReceiveErrorCode::CONNECTION_CLOSED, "Connection closed by peer"});
        }

        return static_cast<size_t>(bytesRead);
    }
}

} // namespace Connections::detail