#include <exception>
#include <string>

#include "sikradio.hpp"

int main(int argc, char* argv[])
{
    try {
        return SikRadio::run(argc, argv);
    } catch (const std::exception& e) {
        SikRadio::logger().critical(std::string{"Unhandled exception: "} + e.what());
        return 1;
    }
}