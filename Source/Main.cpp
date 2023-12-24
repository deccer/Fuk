#include <cstdint>
#include <cstdlib>

#include <iostream>
#include <string>
#include <vector>

#include "ApplicationIcon.hpp"

#include "Io.hpp"
#include "Engine.hpp"

constexpr int32_t MAX_FRAMES_IN_FLIGHT = 3;

int32_t main([[maybe_unused]] int32_t argc, [[maybe_unused]] char* argv[])
{
    Engine engine;
    if (!engine.Initialize())
    {
        return EXIT_FAILURE;
    }

    if (!engine.Load())
    {
        engine.Unload();
        return EXIT_FAILURE;
    }

    //auto currentTime = glfwGetTime();
    //auto previousTime = currentTime;

    while (!glfwWindowShouldClose(engine.GetWindow()))
    {
        glfwPollEvents();
        //currentTime = glfwGetTime();
        //auto deltaTime = static_cast<float>(currentTime - previousTime);
        //previousTime = currentTime;

        if (!engine.Draw())
        {
            break;
        }
    }

    engine.Unload();

    return EXIT_SUCCESS;
}