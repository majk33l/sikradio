#pragma once

#include <expected>
#include <string>
#include <cstdint>

#include "addressFamily.hpp"
#include "logger.hpp"

namespace ArgumentParser
{

    /** Constants specified in the sikradio specification. */
    const long  minTimeoutMs = 100;
    const long  maxTimeoutMs = 100000;
    const short minVerbosity = static_cast<short>(Logging::LogLevel::QUIET);
    const short maxVerbosity = static_cast<short>(Logging::LogLevel::DEBUG);

    /** Convert an address family to the value expected by getaddrinfo. */
    int addressFamilyToInt(Connections::AddressFamily family);

    /** Parsed command-line arguments. */
    struct Config
    {
        std::string url;
        bool multiplex;
        uint32_t timeoutMs;
        uint8_t verbosity;
        Connections::AddressFamily addressFamily;
    };

    /** Parse command-line arguments. */
    std::expected<Config, std::string> parse(int argc, char **argv);

    /** Return usage information for the program. */
    std::string usage(const std::string& programName);

} // namespace ArgumentParser