#include "argParser.hpp"
#include "utils.hpp"

#include <cstring>
#include <expected>
#include <string>
#include <cstdint>
#include <unistd.h>
#include <netdb.h>

namespace ArgumentParser
{

int addressFamilyToInt(Connections::AddressFamily family)
{
    switch (family)
    {
        case Connections::AddressFamily::ANY:  return AF_UNSPEC;
        case Connections::AddressFamily::IPv4: return AF_INET;
        case Connections::AddressFamily::IPv6: return AF_INET6;
        default:                             return AF_UNSPEC; // Default to ANY
    }
}

std::expected<Config, std::string> parse(int argc, char** argv)
{
    Config cfg;

    // Default values ! except for AddressFamily,
    // which is set at the end of the funtion
    cfg.multiplex = false;
    cfg.timeoutMs = 5000;
    cfg.verbosity = 2;

    // Checkers if the option was already specified
    bool urlSet         = false;
    bool timeoutSet     = false;
    bool verbositySet   = false;
    bool ipv6Set        = false;
    bool ipv4Set        = false;

    int opt;

    while ((opt = getopt(argc, argv, "u:mt:46v:q")) != -1)
    {
        switch (opt)
        {
        case 'u':
            if (urlSet)
                return std::unexpected("Option -u specified multiple times");

            cfg.url = optarg;
            urlSet = true;
            break;

        case 'm':
            cfg.multiplex = true;
            break;

        case 't':
        {
            if (timeoutSet)
                return std::unexpected("Option -t specified multiple times");

            auto timeout = StringUtils::parseIntArg(
                optarg,
                minTimeoutMs,
                maxTimeoutMs
            );

            if (!timeout)
                return std::unexpected(std::move(timeout.error()));

            cfg.timeoutMs = static_cast<uint32_t>(*timeout);
            timeoutSet = true;
            break;
        }

        case '4':
            ipv4Set = true;
            break;

        case '6':
            ipv6Set = true;
            break;

        case 'v':
        {
            if (verbositySet)
                return std::unexpected("Option -v specified multiple times");

            auto verbosity = StringUtils::parseIntArg(
                optarg,
                minVerbosity,
                maxVerbosity
            );

            if (!verbosity)
                return std::unexpected(std::move(verbosity.error()));

            cfg.verbosity = static_cast<uint8_t>(*verbosity);
            verbositySet = true;
            break;
        }

        case 'q':
            cfg.verbosity = minVerbosity;
            verbositySet = true;
            break;

        case '?':
            return std::unexpected("Unknown option");

        default:
            return std::unexpected("Failed to parse arguments");
        }
    }

    if (!urlSet)
        return std::unexpected("Missing required option -u");

    // Default address family settings
    if (ipv4Set && !ipv6Set)        cfg.addressFamily = Connections::AddressFamily::IPv4;
    else if (!ipv4Set && ipv6Set)   cfg.addressFamily = Connections::AddressFamily::IPv6;
    else                            cfg.addressFamily = Connections::AddressFamily::ANY;

    return cfg;
}

std::string usage(const std::string& programName)
{
    std::string usageStr;
    usageStr += "Usage: " + programName + " -u <url> [options]\n";
    usageStr += "Options:\n";
    usageStr += "  -u <url>       Specify the URL to connect to (required)\n";
    usageStr += "  -m             Enable multiplexing\n";
    usageStr += "  -t <timeout>   Set timeout in milliseconds (default: 1000, range: 100-100000)\n";
    usageStr += "  -4             Use IPv4 only\n";
    usageStr += "  -6             Use IPv6 only\n";
    usageStr += "  -v <level>     Set verbosity level (default: 2, range: 0-4)\n";
    usageStr += "  -q             Quiet mode (verbosity level 0)\n";

    return usageStr;

}

} // namespace ArgumentParser
