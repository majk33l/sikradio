#pragma once

#include <atomic>
#include <csignal>
#include <iostream>
#include <poll.h>
#include <string>
#include <unistd.h>

namespace Quitter
{

    /** Command specified in the sikradio specification for quitting the program. */
    inline constexpr const char *quitCommand = "quit";

    /** Listen on stdin for the quit command. */
    inline void threadFunction(std::atomic<bool> &quit)
    {
        std::string input;

        pollfd pfd{};
        pfd.fd = STDIN_FILENO;
        pfd.events = POLLIN;

        while (!quit.load())
        {
            int ret = poll(&pfd, 1, 100);

            if (quit.load())
                break;

            if (ret > 0 && (bool) (pfd.revents & POLLIN))
            {
                if (!std::getline(std::cin, input))
                    break; // EOF or stream error.

                if (input == quitCommand)
                {
                    quit.store(true);
                    break;
                }
            }
            else if (ret < 0)
            {
                if (errno == EINTR)
                    continue; // Interrupted by signal.

                break; // Actual error.
            }
        }
    }

} // namespace Quitter