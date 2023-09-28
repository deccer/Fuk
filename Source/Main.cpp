#include <cstdint>
#include <cstdlib>

#include <iostream>

#include "ApplicationIcon.hpp"
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <VkBootstrap.h>

bool InitializeVulkan()
{
    vkb::InstanceBuilder instanceBuilder;
    auto instanceResult = instanceBuilder.set_app_name("Fuk")
        .request_validation_layers()
        .use_default_debug_messenger()
        .build();
    if (!instanceResult)
    {
        std::cerr << "Vulkan: Failed to create instance\nDetails: " << instanceResult.error().message() << "\n";
        return false;
    }
    vkb::Instance vkb_inst = instanceResult.value();

    return true;
}

int32_t main(int32_t argc, char* argv[])
{
    if (!glfwInit())
    {
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    auto primaryMonitor = glfwGetPrimaryMonitor();
    auto videoMode = glfwGetVideoMode(primaryMonitor);

    auto screenWidth = videoMode->width;
    auto screenHeight = videoMode->height;

    auto windowWidth = static_cast<int32_t>(screenWidth * 0.8f);
    auto windowHeight = static_cast<int32_t>(screenHeight * 0.8f);

    auto window = glfwCreateWindow(windowWidth, windowHeight, "Fuk Window", nullptr, nullptr);
    if (window == nullptr)
    {
        std::printf("C++: Unable to create window\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }
    std::cout << "C++: Window Created\n";

    int32_t monitorLeft = 0;
    int32_t monitorTop = 0;
    glfwGetMonitorPos(primaryMonitor, &monitorLeft, &monitorTop);
    glfwSetWindowPos(window, screenWidth / 2 - windowWidth / 2 + monitorLeft, screenHeight / 2 - windowHeight / 2 + monitorTop);

    int32_t appImageWidth{};
    int32_t appImageHeight{};
    unsigned char* appImagePixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(&AppIcon[0]), AppIcon_length, &appImageWidth, &appImageHeight, nullptr, 4);
    if (appImagePixels != nullptr)
    {
        GLFWimage appImage;
        appImage.width = 256;
        appImage.height = 256;
        appImage.pixels = appImagePixels;
        glfwSetWindowIcon(window, 1, &appImage);
        stbi_image_free(appImagePixels);
    }    

    auto currentTime = glfwGetTime();
    auto previousTime = currentTime;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        currentTime = glfwGetTime();
        auto deltaTime = static_cast<float>(currentTime - previousTime);
        previousTime = currentTime;

        
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}