#include <cstddef>
#include <cstdlib>
#include <exception>
#include <iostream>

#include "application.hpp"
#include "launch_options.hpp"

int main(int argc, char** argv)
{
    try
    {
        return advanced_platformer::runApplication(
            advanced_platformer::parseLaunchOptions({argv, static_cast<std::size_t>(argc)}));
    }
    catch (const std::exception& error)
    {
        std::cerr << "Advanced Platformer could not start: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
