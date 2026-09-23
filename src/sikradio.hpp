#pragma once

#include "logger.hpp"

namespace SikRadio
{

/** Return the global sikradio logger. */
Logging::Logger& logger();

/** Set the sikradio logger verbosity level. */
void setLoggerVerbosity(Logging::LogLevel level);

/** Parse arguments, configure logging, and run the main receive loop. */
int run(int argc, char* argv[]);

} // namespace SikRadio