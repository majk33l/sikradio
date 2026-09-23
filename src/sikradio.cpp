#include "sikradio.hpp"

#include <thread>
#include <atomic>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <expected>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "quitterThread.hpp"
#include "argParser.hpp"
#include "url.hpp"
#include "utils.hpp"
#include "logger.hpp"
#include "connectionClient.hpp"
#include "streamProcessor.hpp"


namespace SikRadio
{

// Define the buffer size for connection data
constexpr size_t BUFFER_SIZE = 1024;

// Sikradio machine states
enum class State
{
    Connect,
    SendRequest,
    ReceiveHeaders,
    Stream,
    Reconnect,
    QuitSuccess,
    QuitFailure,
};

// Static logger getter
Logging::Logger& logger()
{
    static Logging::Logger instance;
    return instance;
}

// Static logger verbosity setter
void setLoggerVerbosity(Logging::LogLevel level)
{
    logger().setVerbosity(level);
}

// sikradio detail namespace for private helper functions
namespace detail
{

volatile std::sig_atomic_t sigintReceived = 0;

// SIGINT handler
void handleSigint(int) noexcept
{
    sigintReceived = 1;
}

bool installSignalHandlers()
{
    struct sigaction sigintAction{};
    sigintAction.sa_handler = handleSigint;
    sigemptyset(&sigintAction.sa_mask);

    if (sigaction(SIGINT, &sigintAction, nullptr) != 0)
    {
        logger().critical("Failed to install SIGINT handler: " +
            std::string{std::strerror(errno)});
        return false;
    }

    struct sigaction sigpipeAction{};
    sigpipeAction.sa_handler = SIG_IGN;
    sigemptyset(&sigpipeAction.sa_mask);

    if (sigaction(SIGPIPE, &sigpipeAction, nullptr) != 0)
    {
        logger().critical("Failed to ignore SIGPIPE: " +
            std::string{std::strerror(errno)});
        return false;
    }

    return true;
}

// Struct for keeping obejcts needed for sikradio
struct Runtime
{
    Runtime(const ArgumentParser::Config& configRef, Urls::Url currentUrlArg)
        : config(configRef), currentUrl(std::move(currentUrlArg)) {}

    const ArgumentParser::Config& config;
    Urls::Url currentUrl;
    std::optional<Connections::ConnectionClient> connection;
    HeaderParser headerParser;
    StreamProcessor streamProcessor;
    CookieJar cookieJar;
    std::array<std::byte, BUFFER_SIZE> buffer;
};

bool shouldReconnect(Connections::ReceiveErrorCode code) noexcept
{
    return code == Connections::ReceiveErrorCode::TIMEOUT ||
        code == Connections::ReceiveErrorCode::CONNECTION_CLOSED;
}

std::string errorSuffix(const std::optional<std::string>& message)
{
    return message ? ": " + *message : std::string{};
}

// Functions wrapping each sikradio machine state

State connect(Runtime& runtime)
{
    logger().noncritical("Connecting to " + runtime.currentUrl.host +
        ":" + runtime.currentUrl.port);

    auto result = Connections::ConnectionClient::connect(
        runtime.currentUrl.host,
        runtime.currentUrl.port,
        runtime.currentUrl.scheme == Urls::Scheme::HTTPS,
        runtime.config.timeoutMs,
        runtime.config.addressFamily);

    if (!result)
    {
        if (sigintReceived != 0)
            return State::QuitSuccess;

        logger().critical("Connection failed" + errorSuffix(result.error().msg));
        return State::QuitFailure;
    }

    runtime.connection = std::move(*result);
    logger().noncritical("Connection established");
    return State::SendRequest;
}

State sendRequest(Runtime& runtime)
{
    std::string request = RequestBuilder::build(
        runtime.currentUrl,
        runtime.config,
        runtime.headerParser,
        runtime.cookieJar);

    logger().communication(request);

    auto result = runtime.connection->send(
        std::as_bytes(std::span<const char>{request.data(), request.size()}));

    if (!result)
    {
        if (sigintReceived != 0)
            return State::QuitSuccess;

        if (result.error().code == Connections::SendErrorCode::CONNECTION_CLOSED)
        {
            logger().noncritical("Connection closed while sending request");
            return State::QuitSuccess;
        }

        logger().critical("Request failed" + errorSuffix(result.error().msg));
        return State::QuitFailure;
    }

    return State::ReceiveHeaders;
}

State receiveHeaders(Runtime& runtime)
{
    auto result = runtime.connection->receive(runtime.buffer);
    if (!result)
    {
        if (sigintReceived != 0)
            return State::QuitSuccess;

        if (result.error().code == Connections::ReceiveErrorCode::CONNECTION_CLOSED)
        {
            logger().noncritical("Connection closed by server");
            return State::QuitSuccess;
        }

        if (shouldReconnect(result.error().code))
        {
            logger().noncritical("Stream receive interrupted; reconnecting");
            return State::Reconnect;
        }

        logger().critical("Receiving headers failed" + errorSuffix(result.error().msg));
        return State::QuitFailure;
    }

    runtime.headerParser.append(runtime.buffer.data(), *result);
    if (runtime.headerParser.tooLarge())
    {
        logger().critical("Response headers are too large");
        return State::QuitFailure;
    }
    if (!runtime.headerParser.complete())
        return State::ReceiveHeaders;

    logger().communication(runtime.headerParser.rawHeaders());

    auto responseResult = runtime.headerParser.parse();
    if (!responseResult)
    {
        logger().critical("Failed to parse response headers" +
            errorSuffix(responseResult.error().msg));
        return State::QuitFailure;
    }

    const Response& response = *responseResult;
    runtime.cookieJar.update(response);

    if (response.isRedirect())
    {
        const auto locations = response.headerAll("location");
        if (locations.empty())
        {
            logger().critical("Redirect response does not contain a Location header");
            return State::QuitFailure;
        }

        const std::string redirect = UrlUtils::resolveRedirect(
            runtime.currentUrl,
            locations.front());
        auto redirectUrl = Urls::parse(redirect);
        if (!redirectUrl)
        {
            logger().critical("Invalid redirect URL: " + redirectUrl.error());
            return State::QuitFailure;
        }

        runtime.currentUrl = std::move(*redirectUrl);
        logger().noncritical("Following redirect to " + runtime.currentUrl.host +
            ":" + runtime.currentUrl.port + runtime.currentUrl.path);
        return State::Reconnect;
    }

    if (!response.isSuccess())
    {
        logger().critical("HTTP error: " + std::to_string(response.status) +
            " " + response.statusLine);
        return State::QuitFailure;
    }

    std::optional<size_t> metaInt;
    if (response.hasHeader("icy-metaint"))
    {
        auto metaIntResult = response.metaInt();
        if (!metaIntResult)
        {
            logger().critical("Invalid icy-metaint header" +
                errorSuffix(metaIntResult.error().msg));
            return State::QuitFailure;
        }

        metaInt = *metaIntResult;
    }

    runtime.streamProcessor.reset(metaInt);
    const std::string_view initialStreamData = runtime.headerParser.remainingData();
    if (!runtime.streamProcessor.process(
            initialStreamData.data(),
            initialStreamData.size()))
    {
        logger().critical("Failed to write audio data to stdout");
        return State::QuitFailure;
    }

    return State::Stream;
}

State stream(Runtime& runtime)
{
    auto result = runtime.connection->receive(runtime.buffer);
    if (!result)
    {
        if (sigintReceived != 0)
            return State::QuitSuccess;

        if (result.error().code == Connections::ReceiveErrorCode::CONNECTION_CLOSED)
        {
            logger().noncritical("Connection closed by server");
            return State::QuitSuccess;
        }

        if (shouldReconnect(result.error().code))
        {
            logger().noncritical("Stream receive interrupted; reconnecting");
            return State::Reconnect;
        }

        logger().critical("Receiving stream failed" + errorSuffix(result.error().msg));
        return State::QuitFailure;
    }

    if (!runtime.streamProcessor.process(
            reinterpret_cast<const char*>(runtime.buffer.data()),
            *result))
    {
        logger().critical("Failed to write audio data to stdout");
        return State::QuitFailure;
    }

    return State::Stream;
}

State reconnect(Runtime& runtime)
{
    logger().noncritical("Resetting connection state");
    runtime.connection.reset();
    runtime.headerParser.reset();
    runtime.streamProcessor.reset();
    return State::Connect;
}

// function wrapping one sikradio machine step
State advance(Runtime& runtime, State state)
{
    switch (state)
    {
    case State::Connect:
        return connect(runtime);
    case State::SendRequest:
        return sendRequest(runtime);
    case State::ReceiveHeaders:
        return receiveHeaders(runtime);
    case State::Stream:
        return stream(runtime);
    case State::Reconnect:
        return reconnect(runtime);
    case State::QuitSuccess:
    case State::QuitFailure:
        return state;
    }

    return State::QuitFailure;
}

} // namespace detail


// Main sikradio run function
int run(int argc, char* argv[])
{
    // Parse command line arguments
    auto cfg = ArgumentParser::parse(argc, argv);
    if (!cfg)
    {
        logger().critical(cfg.error());
        logger().raw(Logging::LogLevel::CRITICAL, ArgumentParser::usage(argv[0]));
        return 1;
    }

    // Set the logger specified verbosity level
    logger().setVerbosity(cfg->verbosity);

    // Debug info
    logger().debug(DebugUtils::configPrint(*cfg));

    // Parse URL
    auto parsedUrl = Urls::parse(cfg->url);
    if (!parsedUrl)
    {
        logger().critical(parsedUrl.error());
        return 1;
    }

    logger().debug(DebugUtils::urlPrint(*parsedUrl));

    detail::sigintReceived = 0;
    if (!detail::installSignalHandlers())
        return 1;

    // Defines whether the program should quit or not
    std::atomic<bool> quit{false};

    // Thread that listens for user input to quit the program
    std::thread quitter(Quitter::threadFunction, std::ref(quit));

    detail::Runtime runtime{*cfg, *parsedUrl};
    State state = State::Connect;

    // Machine state loop
    while (state != State::QuitSuccess && state != State::QuitFailure)
    {
        state = detail::advance(runtime, state);

        if (detail::sigintReceived != 0)
            quit.store(true);

        if (quit.load()) state = State::QuitSuccess;
    }

    // Flush reamaining radio data to stdin
    std::cout.flush();

    // Join the listening thread
    if (!quit.load()) quit.store(true);
    quitter.join();

    return state == State::QuitSuccess ? 0 : 1;
}

} // namespace SikRadio