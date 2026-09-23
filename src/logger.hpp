#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace Logging
{

/** Logging level used to filter output verbosity. */
enum class LogLevel : uint8_t {
    QUIET = 0,              // No output.
    COMMUNICATION = 1,      // Protocol details.
    CRITICAL = 2,           // Error conditions.
    NONCRITICAL = 3,        // Warnings and status information.
    DEBUG = 4               // Detailed debug information.
};


/** Logger for diagnostic output to stderr. */
class Logger {
public:

    /** Construct a logger with the given verbosity level. */
    explicit Logger(uint8_t verbosity = 2);

    /** Set the logging verbosity level. */
    void setVerbosity(uint8_t verbosity) noexcept;
    void setVerbosity(LogLevel verbosity) noexcept;

    /** Return the current verbosity level. */
    uint8_t verbosity() const noexcept;

    /** Check whether a log level is enabled. */
    bool enabled(LogLevel level) const noexcept;

    /** Log a protocol-level message with a timestamp. */
    void communication(std::string_view message) const noexcept;

    /** Log an error condition. */
    void critical(std::string_view message) const noexcept;

    /** Log a warning or status message. */
    void noncritical(std::string_view message) const noexcept;

    /** Log a debug-level message. */
    void debug(std::string_view message) const noexcept;

    /** Log a message at the specified level. */
    void log(LogLevel level, std::string_view message) const noexcept;

    /** Output unformatted data when the requested level is enabled. */
    void raw(LogLevel level, std::string_view message) const noexcept;

private:

    uint8_t verbosity_;

    /** Generate the current timestamp. */
    static std::string timestamp() noexcept;

    /** Emit a message with an optional timestamp prefix. */
    static void emit(std::string_view message, bool withTimestamp = false) noexcept;

    /** Output a message without formatting. */
    static void rawEmit(std::string_view message) noexcept;
};

} // namespace Logging