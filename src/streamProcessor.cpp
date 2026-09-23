#include "streamProcessor.hpp"
#include "sikradio.hpp"

#include <algorithm>
#include <iostream>

// StreamProcessor constructor and reset

StreamProcessor::StreamProcessor(
    std::optional<size_t> metaInt
)
    : metaInt_(metaInt)
    , state_(State::Audio)
    , audioRemaining_(metaInt.value_or(0))
    , metaRemaining_(0)
{}

// Reset processor
void StreamProcessor::reset(std::optional<size_t> metaInt)
{
    metaInt_        = metaInt;
    state_          = State::Audio;
    audioRemaining_ = metaInt.value_or(0);
    metaRemaining_  = 0;
    metaBuffer_.clear();
}

// Process stream data chunk
bool StreamProcessor::process(const char* data, size_t size) {
    // No metadata - pass audio to stdout
    if (!metaInt_) {
        std::cout.write(data, static_cast<std::streamsize>(size));
        return static_cast<bool>(std::cout);
    }

    // Process chunk
    while (size > 0) {
        switch (state_) {

        case State::Audio: {
            // Read audio data until metaInt boundary

            if (audioRemaining_ == 0) {
                // Reached metadata position
                state_ = State::MetaSize;
                continue;
            }

            size_t chunk = std::min(size, audioRemaining_);

            std::cout.write(
                data,
                static_cast<std::streamsize>(chunk)
            );

            if (!std::cout)
                return false;

            data += chunk;
            size -= chunk;
            audioRemaining_ -= chunk;

            if (audioRemaining_ == 0)
                state_ = State::MetaSize;

            break;
        }

        case State::MetaSize: {
            // Read 1 byte metadata length (in units of 16 bytes)

            if (size == 0)
                return true;

            metaRemaining_ = static_cast<size_t>(
                static_cast<unsigned char>(*data)
            ) * 16;

            metaBuffer_.clear();

            ++data;
            --size;

            // If metadata size is 0, return to audio
            if (metaRemaining_ == 0) {
                audioRemaining_ = *metaInt_;
                state_ = State::Audio;
            }
            else state_ = State::Meta;

            break;
        }

        case State::Meta: {
            // Read metadata bytes

            size_t chunk = std::min(size, metaRemaining_);

            metaBuffer_.append(data, chunk);

            data += chunk;
            size -= chunk;

            metaRemaining_ -= chunk;

            if (metaRemaining_ == 0) {
                // Metadata complete

                auto end = metaBuffer_.find('\0');

                if (end != std::string::npos)
                    metaBuffer_.resize(end);

                if (!metaBuffer_.empty())
                    SikRadio::logger().raw(
                        Logging::LogLevel::QUIET,
                        metaBuffer_);

                // Return to audio processing
                audioRemaining_ = *metaInt_;
                state_ = State::Audio;
            }

            break;
        }

        }
    }

    return true;
}
