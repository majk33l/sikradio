#include <array>
#include <iostream>
#include <ctime>
#include <chrono>

#include "logger.hpp"
#include "utils.hpp"

// Logger constructor and basic methods

namespace Logging
{

Logger::Logger(uint8_t verbosity)
    : verbosity_(verbosity) {}

void Logger::setVerbosity(uint8_t verbosity) noexcept {
    verbosity_ = verbosity;
}

void Logger::setVerbosity(LogLevel verbosity) noexcept {
    verbosity_ = static_cast<uint8_t>(verbosity);
}

uint8_t Logger::verbosity() const noexcept {
    return verbosity_;
}

bool Logger::enabled(LogLevel level) const noexcept {
    return static_cast<uint8_t>(level) <= verbosity_;
}

// Logging methods for each level

void Logger::communication(std::string_view message) const noexcept{
    log(LogLevel::COMMUNICATION, message);
}

void Logger::critical(std::string_view message) const noexcept{
    log(LogLevel::CRITICAL, message);
}

void Logger::noncritical(std::string_view message) const noexcept{
    log(LogLevel::NONCRITICAL, message);
}

void Logger::debug(std::string_view message) const noexcept{
    log(LogLevel::DEBUG, message);
}

// Central log method - outputs if level is enabled
void Logger::log(LogLevel level, std::string_view message) const noexcept{
    if (enabled(level)) {
        // Communication logs include timestamp
        bool withTimestamp = (level == LogLevel::COMMUNICATION);
        emit(message, withTimestamp);
    }
}

// Raw output without formatting or timestamp
void Logger::raw(LogLevel level, std::string_view message) const noexcept
{
    if (enabled(level))
        rawEmit(StringUtils::rstrip(message));
}

// Private helper methods

// Generate current timestamp in format: YYYY.MM.DD HH.MM.SS
std::string Logger::timestamp() noexcept {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    if (localtime_r(&time, &tm_buf) == nullptr) {
        return {};
    }

    std::array<char, 20> buffer{};
    const auto written = std::strftime(buffer.data(), buffer.size(), "%Y.%m.%d %H.%M.%S", &tm_buf);
    if (written == 0) {
        return {};
    }

    try {
        return std::string(buffer.data(), written);
    } catch (const std::exception&) {
        return {};
    }
}

// Emit message with optional timestamp prefix
void Logger::emit(std::string_view message, bool withTimestamp) noexcept
{
    if (withTimestamp)
        std::cerr << timestamp() << "\n";
    std::cerr << StringUtils::rstrip(message) << "\n\n";
}

// Output message raw
void Logger::rawEmit(std::string_view message) noexcept{
    std::cerr << message << "\n\n";
}

} // namespace Logging