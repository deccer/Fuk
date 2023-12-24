#include "Engine.hpp"
#include "ApplicationIcon.hpp"
#include "Io.hpp"
#include "Stbi.hpp"

#include <filesystem>
#include <stack>
#include <tuple>
#include <format>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/string_cast.hpp>

VkSurfaceKHR CreateSurface(
    VkInstance instance,
    GLFWwindow* window,
    VkAllocationCallbacks* allocator = nullptr)
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkResult result = glfwCreateWindowSurface(instance, window, allocator, &surface);
    if (result)
    {
        const char* errorMessage;
        int errorCode = glfwGetError(&errorMessage);
        if (errorCode != GLFW_NO_ERROR)
        {
            std::cout << errorCode;
            if (errorMessage != nullptr)
            {
                std::cout << " " << errorMessage;
            }
            std::cout << "\n";
        }
        surface = VK_NULL_HANDLE;
    }

    return surface;
}

glm::mat4 NodeToMat4(const fastgltf::Node& node)
{
    glm::mat4 transform{1.0};

    if (auto* trs = std::get_if<fastgltf::Node::TRS>(&node.transform))
    {
        auto rotation = glm::quat{trs->rotation[3], trs->rotation[0], trs->rotation[1], trs->rotation[2]};
        auto scale = glm::vec3{trs->scale[0], trs->scale[1], trs->scale[2]};
        auto translation = glm::vec3{trs->translation[0], trs->translation[1], trs->translation[2]};

        glm::mat4 rotationMat = glm::mat4_cast(rotation);

        // T * R * S
        transform = glm::scale(glm::translate(translation) * rotationMat, scale);
    }
    else if (auto* mat = std::get_if<fastgltf::Node::TransformMatrix>(&node.transform))
    {
        const auto& m = *mat;
        // clang-format off
        transform =
        { 
            m[0], m[1], m[2], m[3], 
            m[4], m[5], m[6], m[7], 
            m[8], m[9], m[10], m[11], 
            m[12], m[13], m[14], m[15],
        };
        // clang-format on
    }

    return transform;
}

bool Engine::Initialize()
{
    if (!glfwInit())
    {
        std::cout << "GLFW: Unable to initialize\n";
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    auto primaryMonitor = glfwGetPrimaryMonitor();
    auto videoMode = glfwGetVideoMode(primaryMonitor);

    auto screenWidth = videoMode->width;
    auto screenHeight = videoMode->height;

    auto windowWidth = static_cast<int32_t>(static_cast<float>(screenWidth) * 0.8f);
    auto windowHeight = static_cast<int32_t>(static_cast<float>(screenHeight) * 0.8f);

    _windowExtent.width = windowWidth;
    _windowExtent.height = windowHeight;

    _window = glfwCreateWindow(windowWidth, windowHeight, _windowTitle.data(), nullptr, nullptr);
    if (_window == nullptr)
    {
        std::cout << "GLFW: Unable to create window\n";
        glfwTerminate();
        return false;
    }

    int32_t monitorLeft = 0;
    int32_t monitorTop = 0;
    glfwGetMonitorPos(primaryMonitor, &monitorLeft, &monitorTop);
    glfwSetWindowPos(_window, screenWidth / 2 - windowWidth / 2 + monitorLeft, screenHeight / 2 - windowHeight / 2 + monitorTop);

    int32_t appImageWidth{};
    int32_t appImageHeight{};
    unsigned char* appImagePixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(&AppIcon[0]), AppIcon_length, &appImageWidth, &appImageHeight, nullptr, 4);
    if (appImagePixels != nullptr)
    {
        GLFWimage appImage{};
        appImage.width = appImageWidth;
        appImage.height = appImageHeight;
        appImage.pixels = appImagePixels;
        glfwSetWindowIcon(_window, 1, &appImage);
        stbi_image_free(appImagePixels);
    }    

    if (!InitializeVulkan())
    {
        return false;
    }

    return true;
}

bool Engine::Load()
{
    return true;
}



bool Engine::Draw()
{
    return true;
}

bool Engine::InitializeVulkan()
{
    if (glfwVulkanSupported() == GLFW_FALSE)
    {
        std::cerr << "Vulkan: Not supported\n";
        return false;
    }

    if (volkInitialize() != VK_SUCCESS)
    {
        std::cerr << "Volk: Failed to initialize\n";
        return false;
    }

    //
    // Initialize Instance
    //

    vkb::InstanceBuilder instanceBuilder;
    auto instanceResult = instanceBuilder
        .set_app_name("Fuk")
        .require_api_version(1, 2, 0)        
#ifdef _DEBUG        
        .request_validation_layers()
        .enable_validation_layers()
        .use_default_debug_messenger()
        .enable_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)        
#endif
        .build();
    if (!instanceResult)
    {
        std::cerr << "Vulkan: Failed to create instance.\nDetails: " << instanceResult.error().message() << "\n";
        return false;
    }

    auto vkbInstance = instanceResult.value();

    _instance = vkbInstance.instance;
#ifdef _DEBUG
    _debugMessenger = vkbInstance.debug_messenger;
#endif

    volkLoadInstance(_instance);

    //
    // Initialize Device
    //

    _surface = CreateSurface(_instance, _window);
    if (_surface == nullptr)
    {
        return false;
    }

    vkb::PhysicalDeviceSelector physicalDeviceSelector{ vkbInstance };
    auto physicalDeviceSelectionResult = physicalDeviceSelector
        .set_surface(_surface)
        .set_minimum_version(1, 2)
        .require_dedicated_transfer_queue()
        .add_required_extension(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME)
        .add_required_extension(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME)
        .select();
    if (!physicalDeviceSelectionResult)
    {
        std::cerr << "Vulkan: Failed to select physical device.\nDetails: " << physicalDeviceSelectionResult.error().message() << "\n";
        return false;
    }

    vkb::DeviceBuilder deviceBuilder{ physicalDeviceSelectionResult.value() };

    VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawParametersFeatures = {};
    shaderDrawParametersFeatures.sType = VkStructureType::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
    shaderDrawParametersFeatures.pNext = nullptr;
    shaderDrawParametersFeatures.shaderDrawParameters = VK_TRUE;

    auto deviceBuilderResult = deviceBuilder
        .add_pNext(&shaderDrawParametersFeatures)
        .build();
    if (!deviceBuilderResult)
    {
        std::cerr << "Vulkan: Failed to create vulkan device.\nDetails: " << deviceBuilderResult.error().message() << "\n";
        return false;
    }

    auto vkbDevice = deviceBuilderResult.value();

    _device = vkbDevice.device;
    _physicalDevice = vkbDevice.physical_device;
    _physicalDeviceProperties = vkbDevice.physical_device.properties;

    volkLoadDevice(_device);

    _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    SetDebugName(_device, _graphicsQueue, "Graphics Queue");    

    if (vmaCreateAllocator(
        ToTempPtr(VmaAllocatorCreateInfo
        {
            .physicalDevice = _physicalDevice,
            .device = _device,
            .pVulkanFunctions = ToTempPtr(VmaVulkanFunctions
            {
                .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
                .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
            }),
            .instance = _instance,
        }),
        &_allocator) != VK_SUCCESS)
    {
        std::cerr << "VirtualMemoryAllocator: Failed to create allocator\n";
        return false;
    }

    return true;
}

void Engine::Unload()
{
    vkDeviceWaitIdle(_device);

    _deletionQueue.Flush();
    
    vmaDestroyAllocator(_allocator);    

    vkDestroySurfaceKHR(_instance, _surface, nullptr);    
    vkDestroyDevice(_device, nullptr);

#ifdef _DEBUG
    vkb::destroy_debug_utils_messenger(_instance, _debugMessenger);
#endif
    vkDestroyInstance(_instance, nullptr);

    if (_window != nullptr)
    {
        glfwDestroyWindow(_window);
    }
    glfwTerminate();
}



GLFWwindow* Engine::GetWindow()
{
    return _window;
}
