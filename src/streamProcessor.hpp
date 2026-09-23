#pragma once

#include <cstddef>
#include <optional>
#include <string>

/** Processes incoming audio data and optional metadata across multiple chunks. */
class StreamProcessor {
public:
    /** Construct a processor with optional metadata support. */
    StreamProcessor(std::optional<size_t> metaInt = std::nullopt);

    /** Process a chunk of incoming stream data. */
    bool process(const char* data, size_t size);

    /** Reset the processor for a new stream. */
    void reset(std::optional<size_t> metaInt = std::nullopt);

private:
    /** State machine for parsing audio and metadata. */
    enum class State {
        Audio,      // Reading audio data.
        MetaSize,   // Reading the metadata size byte.
        Meta        // Reading metadata.
    };

    std::optional<size_t> metaInt_;     // Audio chunk size before metadata.

    State state_;                       // Current parsing state.
    size_t audioRemaining_;             // Bytes of audio left before metadata.
    size_t metaRemaining_;              // Bytes of metadata left to read.
    std::string metaBuffer_;            // Accumulated metadata.
};
