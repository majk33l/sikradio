#include "connectionTls.hpp"

#include <openssl/err.h>

#include <cerrno>
#include <cstring>
#include <string>
#include <utility>

namespace Connections::detail
{

namespace
{

std::string getOpenSslErrorMsg(const char* fallback) noexcept
{
    unsigned long errCode = ::ERR_get_error();
    if (errCode == 0)
    {
        return fallback;
    }
    char buf[256];
    ::ERR_error_string_n(errCode, buf, sizeof(buf));
    return std::string{buf};
}

} // namespace

std::expected<TlsConnection, ConnectionError> TlsConnection::connect(
    std::string_view host,
    std::string_view port,
    AddressFamily addressFamily)
{
    auto tcpRes = TcpConnection::connect(host, port, addressFamily);
    if (!tcpRes)
    {
        return std::unexpected(tcpRes.error());
    }

    SslCtxPtr ctx{::SSL_CTX_new(::TLS_client_method())};
    if (!ctx)
    {
        return std::unexpected(ConnectionError{
            ConnectionErrorCode::TLS_HANDSHAKE_FAILED,
            getOpenSslErrorMsg("Failed to create SSL_CTX")});
    }


    ::SSL_CTX_set_verify(ctx.get(), SSL_VERIFY_PEER, nullptr);
    if (::SSL_CTX_set_default_verify_paths(ctx.get()) != 1)
    {
        return std::unexpected(ConnectionError{
            ConnectionErrorCode::TLS_HANDSHAKE_FAILED,
            getOpenSslErrorMsg("Failed to set default CA verify paths")});
    }

    SslPtr ssl{::SSL_new(ctx.get())};
    if (!ssl)
    {
        return std::unexpected(ConnectionError{
            ConnectionErrorCode::TLS_HANDSHAKE_FAILED,
            getOpenSslErrorMsg("Failed to create SSL object")});
    }

    std::string serverName{host};
    if (serverName.size() >= 2 &&
        serverName.front() == '[' && serverName.back() == ']')
    {
        serverName = serverName.substr(1, serverName.size() - 2);
    }

    if (::SSL_set_tlsext_host_name(ssl.get(), serverName.c_str()) != 1 ||
        ::X509_VERIFY_PARAM_set1_host(
            ::SSL_get0_param(ssl.get()),
            serverName.c_str(),
            0) != 1)
    {
        return std::unexpected(ConnectionError{
            ConnectionErrorCode::TLS_HANDSHAKE_FAILED,
            "Failed to configure TLS server name verification"});
    }

    if (::SSL_set_fd(ssl.get(), tcpRes->descriptor()) != 1)
    {
        return std::unexpected(ConnectionError{
            ConnectionErrorCode::TLS_HANDSHAKE_FAILED,
            getOpenSslErrorMsg("Failed to assign socket fd to SSL object")});
    }

    while (true)
    {
        ::ERR_clear_error();
        errno = 0;
        int res = ::SSL_connect(ssl.get());

        if (res == 1)
        {
            break;
        }

        int err = ::SSL_get_error(ssl.get(), res);

        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return std::unexpected(ConnectionError{
                ConnectionErrorCode::TLS_HANDSHAKE_FAILED,
                "TLS handshake socket error"});
        }

        if (err == SSL_ERROR_SYSCALL)
        {
            if (errno == EINTR)
            {
                continue;
            }
            std::string msg = (errno != 0) ? ::strerror(errno) : "TLS handshake syscall error";
            return std::unexpected(ConnectionError{
                ConnectionErrorCode::TLS_HANDSHAKE_FAILED,
                std::move(msg)});
        }

        return std::unexpected(ConnectionError{
            ConnectionErrorCode::TLS_HANDSHAKE_FAILED,
            getOpenSslErrorMsg("TLS handshake failed")});
    }

    return TlsConnection{std::move(*tcpRes), std::move(ctx), std::move(ssl)};
}

std::expected<void, ConnectionError> TlsConnection::setReceiveTimeout(uint32_t timeoutMs) noexcept
{
    return tcp_.setReceiveTimeout(timeoutMs);
}

std::expected<void, SendError> TlsConnection::send(std::span<const std::byte> data)
{
    size_t totalSent = 0;

    while (totalSent < data.size())
    {
        ::ERR_clear_error();
        errno = 0;

        int sent = ::SSL_write(
            ssl_.get(),
            reinterpret_cast<const char*>(data.data()) + totalSent,
            static_cast<int>(data.size() - totalSent));

        if (sent > 0)
        {
            totalSent += static_cast<size_t>(sent);
            continue;
        }

        int err = ::SSL_get_error(ssl_.get(), sent);

        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
        {
            if (errno == EINTR)
            {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return std::unexpected(SendError{
                    SendErrorCode::SEND_FAILED,
                    "TLS send socket would block"});
            }
        }

        if (err == SSL_ERROR_SYSCALL)
        {
            if (errno == EINTR)
            {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return std::unexpected(SendError{
                    SendErrorCode::SEND_FAILED,
                    "TLS send socket would block"});
            }
            std::string msg = (errno != 0) ? ::strerror(errno) : "TLS send syscall error";
            return std::unexpected(SendError{
                SendErrorCode::SEND_FAILED,
                std::move(msg)});
        }

        if (err == SSL_ERROR_ZERO_RETURN)
        {
            return std::unexpected(SendError{
                SendErrorCode::CONNECTION_CLOSED,
                "TLS connection closed while sending"});
        }

        return std::unexpected(SendError{
            SendErrorCode::SEND_FAILED,
            getOpenSslErrorMsg("TLS write error")});
    }

    return {};
}

std::expected<size_t, ReceiveError> TlsConnection::receive(std::span<std::byte> buffer)
{
    if (buffer.empty())
    {
        return 0;
    }

    while (true)
    {
        ::ERR_clear_error();
        errno = 0;

        int bytesRead = ::SSL_read(
            ssl_.get(),
            buffer.data(),
            static_cast<int>(buffer.size()));

        if (bytesRead > 0)
        {
            return static_cast<size_t>(bytesRead);
        }

        int err = ::SSL_get_error(ssl_.get(), bytesRead);

        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
        {
            if (errno == EINTR)
            {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return std::unexpected(ReceiveError{
                    ReceiveErrorCode::TIMEOUT,
                    "TLS receive socket timeout exceeded"});
            }
        }

        if (err == SSL_ERROR_SYSCALL)
        {
            if (errno == EINTR)
            {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return std::unexpected(ReceiveError{
                    ReceiveErrorCode::TIMEOUT,
                    "TLS receive socket timeout exceeded"});
            }
            std::string msg = (errno != 0) ? ::strerror(errno) : "TLS receive syscall error";
            return std::unexpected(ReceiveError{
                ReceiveErrorCode::RECEIVE_FAILED,
                std::move(msg)});
        }

        if (err == SSL_ERROR_ZERO_RETURN)
        {
            return std::unexpected(ReceiveError{
                ReceiveErrorCode::CONNECTION_CLOSED,
                "TLS connection closed by remote peer"});
        }

        return std::unexpected(ReceiveError{
            ReceiveErrorCode::RECEIVE_FAILED,
            getOpenSslErrorMsg("TLS read error")});
    }
}

} // namespace Connections::detail