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

#include <spdlog/spdlog.h>

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
            spdlog::error("{}: Unable to create window surface. {} - {}", "Vulkan", errorCode, errorMessage);
        }
        surface = VK_NULL_HANDLE;
    }

    return surface;
}

VkCommandPoolCreateInfo CreateCommandPoolCreateInfo(
    uint32_t queueFamilyIndex,
    VkCommandPoolCreateFlags flags)
{
    return
    {
        .sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags,
        .queueFamilyIndex = queueFamilyIndex,
    };
}

VkCommandBufferAllocateInfo CreateCommandBufferAllocateInfo(
    VkCommandPool pool,
    uint32_t count = 1)
{
    return
    {
        .sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = pool,
        .level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY,        
        .commandBufferCount = count,
    };
}

VkFenceCreateInfo CreateFenceCreateInfo(VkFenceCreateFlags flags)
{
    return
    {
        .sType = VkStructureType::VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags
    };
}

VkSemaphoreCreateInfo CreateSemaphoreCreateInfo(VkSemaphoreCreateFlags flags)
{
    return
    {
        .sType = VkStructureType::VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags,
    };
}

VkCommandBufferBeginInfo CreateCommandBufferBeginInfo(VkCommandBufferUsageFlags flags)
{
    return
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = flags,
        .pInheritanceInfo = nullptr,
    };
}

VkImageSubresourceRange CreateImageSubresourceRange(VkImageAspectFlags imageAspectMask)
{
    return
    {
        .aspectMask = imageAspectMask,
        .baseMipLevel = 0,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };
}

void TransitionImage(
    VkCommandBuffer commandBuffer,
    VkImage image,
    VkImageLayout currentImageLayout,
    VkImageLayout newImageLayout)
{
    VkDependencyInfo dependencyInfo =
    {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = ToTempPtr(VkImageMemoryBarrier2
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
            .oldLayout = currentImageLayout,
            .newLayout = newImageLayout,
            .image = image,
            .subresourceRange = CreateImageSubresourceRange(newImageLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
        })
    };

    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}

VkSemaphoreSubmitInfo CreateSemaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore)
{
    return
    {
        .sType = VkStructureType::VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext = nullptr,
        .semaphore = semaphore,
        .value = 1,
        .stageMask = stageMask,
        .deviceIndex = 0,
    };
}

VkCommandBufferSubmitInfo CreateCommandBufferSubmitInfo(VkCommandBuffer commandBuffer)
{
    return
    {
        .sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = nullptr,
        .commandBuffer = commandBuffer,
        .deviceMask = 0,
    };
}

VkSubmitInfo2 CreateSubmitInfo(
    VkCommandBufferSubmitInfo* commandBufferSubmitInfo,
    VkSemaphoreSubmitInfo* signalSemaphoreInfo,
    VkSemaphoreSubmitInfo* waitSemaphoreInfo)
{
    return
    {
        .sType = VkStructureType::VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,

        .waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0 : 1,
        .pWaitSemaphoreInfos = waitSemaphoreInfo,

        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = commandBufferSubmitInfo,        

        .signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0 : 1,
        .pSignalSemaphoreInfos = signalSemaphoreInfo,
    };
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

    if (!CreateSwapchain(_swapchainExtent.width, _swapchainExtent.height))
    {
        return false;
    }

    if (!InitializeSynchronizationStructures())
    {
        return false;
    }

    if (!InitializeCommands())
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
    auto currentFrameData = GetCurrentFrameData();
    if (vkWaitForFences(_device, 1, &currentFrameData.renderFence, true, UINT64_MAX) != VK_SUCCESS)
    {
        spdlog::error("Vulkan: Failed to wait for render fence");
        return false;
    }

    if (vkResetFences(_device, 1, &currentFrameData.renderFence) != VK_SUCCESS)
    {
        spdlog::error("Vulkan: Failed to reset render fence");
        return false;
    }

    uint32_t swapchainImageIndex;
    if (vkAcquireNextImageKHR(_device, _swapchain, UINT64_MAX, currentFrameData.presentSemaphore, nullptr, &swapchainImageIndex) != VK_SUCCESS)
    {
        spdlog::error("Vulkan: Unable to acquire next image");
        return false;
    }

    auto commandBuffer = currentFrameData.mainCommandBuffer;
    if (vkResetCommandBuffer(commandBuffer, 0) != VK_SUCCESS)
    {
        spdlog::error("Vulkan: Unable to reset main command buffer");
        return false;
    }

    auto commandBufferBeginInfo = CreateCommandBufferBeginInfo(VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    if (vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo) != VK_SUCCESS)
    {
        spdlog::error("Vulkan: Unable to begin command buffer");
        return false;
    }

    TransitionImage(commandBuffer, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    VkClearColorValue clearValue = { { 0.2f, 0.0f, 0.3f, 1.0f } };

    VkImageSubresourceRange clearRange = CreateImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
    vkCmdClearColorImage(commandBuffer, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

    TransitionImage(commandBuffer, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        spdlog::error("Vulkan: Unable to end command buffer");
        return false;
    }

    auto commandBufferSubmitInfo = CreateCommandBufferSubmitInfo(commandBuffer);
    auto signalInfo = CreateSemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, currentFrameData.renderSemaphore);	
    auto semaphoreSubmitInfo = CreateSemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, currentFrameData.presentSemaphore);
    
    auto submitInfo = CreateSubmitInfo(&commandBufferSubmitInfo, &signalInfo, &semaphoreSubmitInfo);	

    if (vkQueueSubmit2(_graphicsQueue, 1, &submitInfo, currentFrameData.renderFence) != VK_SUCCESS)
    {
        spdlog::error("Vulkan: Unable to submit to graphics queue");
        return false;
    }

    auto presentInfo = VkPresentInfoKHR
    {
        .sType = VkStructureType::VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &currentFrameData.renderSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &_swapchain,
        .pImageIndices = &swapchainImageIndex
    };

    if (vkQueuePresentKHR(_graphicsQueue, &presentInfo) != VK_SUCCESS)
    {
        spdlog::error("Vulkan: Unable to present queue");
        return false;
    }

    _frameNumber++;

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
        .require_api_version(1, 3, 0)        
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

    VkPhysicalDeviceVulkan13Features features13 = {};
    features13.dynamicRendering = true;
    features13.synchronization2 = true;

    VkPhysicalDeviceVulkan12Features features12 = {};
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    vkb::PhysicalDeviceSelector physicalDeviceSelector{ vkbInstance };
    auto physicalDeviceSelectionResult = physicalDeviceSelector

        .set_minimum_version(1, 3)
        .set_required_features_12(features12)
        .set_required_features_13(features13)
        .set_surface(_surface)
        .select();
    if (!physicalDeviceSelectionResult)
    {
        spdlog::error("Vulkan: Failed to select physical device.\nDetails: {}", physicalDeviceSelectionResult.error().message());
        return false;
    }

    vkb::PhysicalDevice vkbPhysicalDevice = physicalDeviceSelectionResult.value();

    vkb::DeviceBuilder deviceBuilder{ vkbPhysicalDevice };
    auto deviceBuilderResult = deviceBuilder.build();
    if (!deviceBuilderResult)
    {
        spdlog::error("Vulkan: Unable to build logical device.\nDetails: {}", deviceBuilderResult.error().message());
        return false;
    }

    auto vkbDevice = deviceBuilderResult.value();
    _physicalDevice = vkbPhysicalDevice.physical_device;
    _device = vkbDevice.device;

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

bool Engine::InitializeCommands()
{
    auto commandPoolCreateInfo = CreateCommandPoolCreateInfo(_graphicsQueueFamily, VkCommandPoolCreateFlagBits::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    for (uint32_t i = 0; i < FRAME_OVERLAP; i++)
    {
        if (vkCreateCommandPool(
            _device,
            &commandPoolCreateInfo,
            nullptr,
            &_frames[i].commandPool) != VK_SUCCESS)
        {
            spdlog::error("{}: Unable to create command pool", "Vulkan");
            return false;
        }

        auto commandBufferAllocateInfo = CreateCommandBufferAllocateInfo(_frames[i].commandPool, 1);
        if (vkAllocateCommandBuffers(_device, &commandBufferAllocateInfo, &_frames[i].mainCommandBuffer) != VK_SUCCESS)
        {
            spdlog::error("{}: Unable to allocate command buffer", "Vulkan");
            return false;
        }
    }

    return true;
}

bool Engine::InitializeSynchronizationStructures()
{
    auto fenceCreateInfo = CreateFenceCreateInfo(VkFenceCreateFlagBits::VK_FENCE_CREATE_SIGNALED_BIT);
    auto semaphoreCreateInfo = CreateSemaphoreCreateInfo(0);

    for (uint32_t i = 0; i < FRAME_OVERLAP; i++)
    {
        if (vkCreateFence(
            _device,
            &fenceCreateInfo,
            nullptr,
            &_frames[i].renderFence
        ) != VK_SUCCESS)
        {
            spdlog::error("Vulkan: Unable to create render fence");
            return false;
        }

        if (vkCreateSemaphore(
            _device,
            &semaphoreCreateInfo,
            nullptr,
            &_frames[i].presentSemaphore
        ) != VK_SUCCESS)
        {
            spdlog::error("Vulkan: Unable to create swapchain semaphore");
            return false;
        }

        if (vkCreateSemaphore(
            _device,
            &semaphoreCreateInfo,
            nullptr,
            &_frames[i].renderSemaphore
        ) != VK_SUCCESS)
        {
            spdlog::error("Vulkan: Unable to create render semaphore");
            return false;
        }
    }
    return true;
}

bool Engine::CreateSwapchain(uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder swapchainBuilder
    {
        _physicalDevice,
        _device,
        _surface
    };

    _swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    auto swapchainResult = swapchainBuilder
        .set_desired_format(VkSurfaceFormatKHR
        {
            .format = _swapchainImageFormat,
            .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        })
        //use vsync present mode
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build();
    if (!swapchainResult)
    {
        spdlog::error("Vulkan: Unable to build swapchain.\nDetails: {}", swapchainResult.error().message());
        return false;
    }

    auto vkbSwapchain = swapchainResult.value();

    _swapchainExtent = vkbSwapchain.extent;
    _swapchain = vkbSwapchain.swapchain;
    _swapchainImages = vkbSwapchain.get_images().value();
    _swapchainImageViews = vkbSwapchain.get_image_views().value();

    return true;
}

void Engine::DestroySwapchain()
{
    vkDestroySwapchainKHR(_device, _swapchain, nullptr);
    for (auto& swapchainImageView : _swapchainImageViews)
    {
        vkDestroyImageView(_device, swapchainImageView, nullptr);
    }
}

FrameData& Engine::GetCurrentFrameData()
{
    return _frames[_frameNumber % FRAME_OVERLAP];
};

void Engine::Unload()
{
    vkDeviceWaitIdle(_device);

    DestroySwapchain();

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
