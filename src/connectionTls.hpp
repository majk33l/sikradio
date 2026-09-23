#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string_view>

#include <openssl/ssl.h>

#include "addressFamily.hpp"
#include "connectionError.hpp"
#include "connectionTcp.hpp"

/** Internal connection implementations. */
namespace Connections::detail
{

/** Custom deleter for an SSL context. */
struct SslCtxDeleter
{ void operator()(SSL_CTX* p) const { SSL_CTX_free(p); } };

struct SslDeleter
{ void operator()(SSL* p) const { SSL_free(p); } };

/** Unique pointer types for SSL objects. */
using SslCtxPtr = std::unique_ptr<SSL_CTX, SslCtxDeleter>;
using SslPtr = std::unique_ptr<SSL, SslDeleter>;

/** Wrapper for a TLS connection. */
class TlsConnection
{
public:
    /** Create a TLS connection, or return a ConnectionError on failure. */
    static std::expected<TlsConnection, ConnectionError> connect(
        std::string_view host,
        std::string_view port,
        AddressFamily addressFamily = AddressFamily::ANY);

    TlsConnection(TlsConnection&&) noexcept = default;
    TlsConnection& operator=(TlsConnection&&) noexcept = default;
    TlsConnection(const TlsConnection&) = delete;
    TlsConnection& operator=(const TlsConnection&) = delete;
    ~TlsConnection() = default;

    /** Send all bytes, or return a SendError on failure. */
    std::expected<void, SendError> send(std::span<const std::byte> data);

    /** Receive bytes, waiting for the configured timeout when no data is available. */
    std::expected<size_t, ReceiveError> receive(std::span<std::byte> buffer);

    /** Set the timeout for receive operations. */
    std::expected<void, ConnectionError> setReceiveTimeout(uint32_t timeoutMs) noexcept;

private:
    /** Construct a TLS connection from its underlying resources. */
    TlsConnection(TcpConnection tcp, SslCtxPtr ctx, SslPtr ssl)
        : tcp_(std::move(tcp)), ctx_(std::move(ctx)), ssl_(std::move(ssl)) {}

    /** Underlying TCP connection. */
    TcpConnection tcp_;

    /** SSL objects used for encryption. */
    SslCtxPtr ctx_;
    SslPtr ssl_;
};

} // namespace Connections::detail