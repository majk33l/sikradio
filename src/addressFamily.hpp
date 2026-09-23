#pragma once

#include <cstdint>

namespace Connections
{

/** Address family used when establishing a connection. */
enum class AddressFamily : uint8_t
{
    ANY,
    IPv4,
    IPv6
};

} // namespace Connections
