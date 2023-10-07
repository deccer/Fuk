#include "Engine.hpp"
#include "ApplicationIcon.hpp"

#include "Io.hpp"
#include "Stbi.hpp"
#include "PipelineBuilder.hpp"

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
        appImage.width = 256;
        appImage.height = 256;
        appImage.pixels = appImagePixels;
        glfwSetWindowIcon(_window, 1, &appImage);
        stbi_image_free(appImagePixels);
    }    

    if (!InitializeVulkan())
    {
        return false;
    }

    if (!InitializeSwapchain())
    {
        return false;
    }

    if (!InitializeCommandBuffers())
    {
        return false;
    }

    if (!InitializeRenderPass())
    {
        return false;
    }

    if (!InitializeFramebuffers())
    {
        return false;
    }

    if (!InitializeSynchronizationStructures())
    {
        return false;
    }

    if (!InitializePipelines())
    {
        return false;
    }

    return true;
}

bool Engine::Load()
{
    if (!TryLoadShaderModule("data/shaders/Simple.vs.glsl.spv", &_simpleVertexShaderModule))
    {
        std::cout << "Engine: Failed to load Simple.vs.glsl.spv\n";
        return false;
    }
    if (!TryLoadShaderModule("data/shaders/Simple.fs.glsl.spv", &_simpleFragmentShaderModule))
    {
        std::cout << "Engine: Failed to load Simple.fs.glsl.spv\n";
        return false;
    }

    VkViewport viewport = {.x = 0, .y = 0, .width = (float)_windowExtent.width, .height = (float)_windowExtent.height, .minDepth = 0.0f, .maxDepth = 1.0f};
    VkRect2D scissor = { .offset = { 0, 0 }, .extent = _windowExtent };

    PipelineBuilder pipelineBuilder(_deletionQueue);
    if (!pipelineBuilder
        .WithGraphicsShadingStages(_simpleVertexShaderModule, _simpleFragmentShaderModule)
        .WithVertexInput()
        .WithTopology(VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .WithPolygonMode(VkPolygonMode::VK_POLYGON_MODE_FILL)
        .WithViewportAndScissor(viewport, scissor)        
        .WithoutBlending()
        .WithoutMultisampling()
        .ForPipelineLayout(_trianglePipelineLayout)
        .TryBuild(_device, _renderPass, &_trianglePipeline))
    {
        std::cout << "Engine: Failed to build pipeline\n";
        return false;
    }

    return true;
}

bool Engine::Draw()
{
    VkResult result = vkWaitForFences(_device, 1, &_renderFence, true, 1000000000);
    if (result != VK_SUCCESS)
    {
        std::cerr << "Vulkan: Unable to wait for render fence\n" << result << "\n";
        return false;
    }

    if (vkResetFences(_device, 1, &_renderFence) != VK_SUCCESS)
    {
        std::cerr << "Vulkan: Unable to reset render fence\n";
        return false;
    }

    uint32_t swapchainImageIndex;
    if (vkAcquireNextImageKHR(
        _device,
        _swapchain,
        1000000000,
        _presentSemaphore,
        nullptr,
        &swapchainImageIndex) != VK_SUCCESS)
    {
        std::cerr << "Vulkan: Unable to acquire next image\n";
        return false;
    }

    if (vkResetCommandBuffer(_mainCommandBuffer, 0) != VK_SUCCESS)
    {
        std::cerr << "Vulkan: Unable to reset command buffer\n";
        return false;
    }

    VkCommandBufferBeginInfo commandBufferBeginInfo = {};
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.pNext = nullptr;
    commandBufferBeginInfo.pInheritanceInfo = nullptr;
    commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(_mainCommandBuffer, &commandBufferBeginInfo) != VK_SUCCESS)
    {
        std::cerr << "Vulkan: Failed to begin command buffer\n";
        return false;
    }

    VkClearValue clearValue;
    float flash = abs(sin(_frameIndex / 120.f));
    clearValue.color = { { 0.0f, 0.0f, flash, 1.0f } };

    VkRenderPassBeginInfo renderPassBeginInfo = {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.pNext = nullptr;

    renderPassBeginInfo.renderPass = _renderPass;
    renderPassBeginInfo.renderArea.offset.x = 0;
    renderPassBeginInfo.renderArea.offset.y = 0;
    renderPassBeginInfo.renderArea.extent = _windowExtent;
    renderPassBeginInfo.framebuffer = _framebuffers[swapchainImageIndex];

    //connect clear values
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(_mainCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);    

    vkCmdBindPipeline(_mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _trianglePipeline);
    vkCmdDraw(_mainCommandBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(_mainCommandBuffer);

    if (vkEndCommandBuffer(_mainCommandBuffer) != VK_SUCCESS)
    {
        std::cerr << "Vulkan: Failed to end command buffer\n";
        return false;
    }

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;

    VkPipelineStageFlags waitStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    submitInfo.pWaitDstStageMask = &waitStageFlags;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &_presentSemaphore;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &_renderSemaphore;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_mainCommandBuffer;

    if (vkQueueSubmit(_graphicsQueue, 1, &submitInfo, _renderFence) != VK_SUCCESS)
    {
        std::cerr << "Vulkan: Failed to submit to queue\n";
        return false;
    }

    // present

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &_swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &_renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pImageIndices = &swapchainImageIndex;

    if (vkQueuePresentKHR(_graphicsQueue, &presentInfo) != VK_SUCCESS)
    {
        std::cerr << "Vulkan: Failed to present\n";
        return false;
    }

    _frameIndex++;

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
    auto deviceBuilderResult = deviceBuilder.build();
    if (!deviceBuilderResult)
    {
        std::cerr << "Vulkan: Failed to create vulkan device.\nDetails: " << deviceBuilderResult.error().message() << "\n";
        return false;
    }

    auto vkbDevice = deviceBuilderResult.value();

    _device = vkbDevice.device;
    _physicalDevice = vkbDevice.physical_device;

    volkLoadDevice(_device);

    _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    return true;
}

bool Engine::InitializeSwapchain()
{
    vkb::SwapchainBuilder swapchainBuilder{ _physicalDevice, _device, _surface };
    auto swapchainBuilderResult = swapchainBuilder
        .use_default_format_selection()
        .set_desired_present_mode(_vsync ? VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR : VkPresentModeKHR::VK_PRESENT_MODE_IMMEDIATE_KHR)
        .set_desired_extent(_windowExtent.width, _windowExtent.height)
        .build();
    if (!swapchainBuilderResult)
    {
        std::cout << swapchainBuilderResult.error().message() << " " << swapchainBuilderResult.vk_result() << "\n";
        return false;
    }

    auto vkbSwapchain = swapchainBuilderResult.value();

    _swapchain = vkbSwapchain.swapchain;
    _swapchainImages = vkbSwapchain.get_images().value();
    _swapchainImageViews = vkbSwapchain.get_image_views().value();
    _swapchainImageFormat = vkbSwapchain.image_format;

    SetDebugName(VkObjectType::VK_OBJECT_TYPE_SWAPCHAIN_KHR, _swapchain, "SwapChain");

    return true;
}

bool Engine::InitializeCommandBuffers()
{
    VkCommandPoolCreateInfo commandPoolCreateInfo = {};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.pNext = nullptr;
    commandPoolCreateInfo.queueFamilyIndex = _graphicsQueueFamily;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(
        _device,
        &commandPoolCreateInfo,
        nullptr,
        &_commandPool) != VK_SUCCESS)
    {
        std::cout << "Vulkan: Failed to create command pool\n";
        return false;
    }

    SetDebugName(VkObjectType::VK_OBJECT_TYPE_COMMAND_POOL, _commandPool, "CommandPool");

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.pNext = nullptr;
    commandBufferAllocateInfo.commandPool = _commandPool;
    commandBufferAllocateInfo.commandBufferCount = 1;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    if(vkAllocateCommandBuffers(
       _device,
       &commandBufferAllocateInfo,
       &_mainCommandBuffer) != VK_SUCCESS)
    {
        std::cout << "Vulkan: Failed to create command buffer\n";
        return false;
    }

    SetDebugName(VkObjectType::VK_OBJECT_TYPE_COMMAND_BUFFER, _mainCommandBuffer, "MainCommandBuffer");

    return true;
}

bool Engine::InitializeRenderPass()
{
    VkAttachmentDescription colorAttachmentDescription = {};
    colorAttachmentDescription.format = _swapchainImageFormat;
    colorAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    colorAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentReference = {};
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorAttachmentReference;

    VkRenderPassCreateInfo renderPassCreateInfo = {};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.pAttachments = &colorAttachmentDescription;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpassDescription;

    if (vkCreateRenderPass(_device, &renderPassCreateInfo, nullptr, &_renderPass) != VK_SUCCESS)
    {
        std::cerr << "Vulkan: Failed to create default render pass\n";
        return false;
    }

    return true;
}

bool Engine::InitializeFramebuffers()
{
    //create the framebuffers for the swapchain images.
    //This will connect the render-pass to the images for rendering

    VkFramebufferCreateInfo framebufferCreateInfo = {};
    framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferCreateInfo.pNext = nullptr;

    framebufferCreateInfo.renderPass = _renderPass;
    framebufferCreateInfo.attachmentCount = 1;
    framebufferCreateInfo.width = _windowExtent.width;
    framebufferCreateInfo.height = _windowExtent.height;
    framebufferCreateInfo.layers = 1;

    auto swapchainImageCount = _swapchainImages.size();
    _framebuffers = std::vector<VkFramebuffer>(swapchainImageCount);

    for (size_t i = 0; i < swapchainImageCount; i++)
    {
        framebufferCreateInfo.pAttachments = &_swapchainImageViews[i];
        if(vkCreateFramebuffer(_device, &framebufferCreateInfo, nullptr, &_framebuffers[i]) != VK_SUCCESS)
        {
            return false;
        }
    }

    return true;
}

bool Engine::InitializeSynchronizationStructures()
{
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = nullptr;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateFence(_device, &fenceCreateInfo, nullptr, &_renderFence) != VK_SUCCESS)
    {
        std::cerr << "Vulkan: Failed to create render fence\n";
        return false;
    }

    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = nullptr;
    semaphoreCreateInfo.flags = 0;

    if (vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_presentSemaphore) != VK_SUCCESS)
    {
        std::cerr << "Vulkan: Failed to create present semaphore\n";
        return false;
    }

    if (vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_renderSemaphore) != VK_SUCCESS)
    {
        std::cerr << "Vulkan: Failed to create render semaphore\n";
        return false;
    }

    return true;
}

bool Engine::InitializePipelines()
{
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext = nullptr;

    pipelineLayoutCreateInfo.flags = 0;
    pipelineLayoutCreateInfo.setLayoutCount = 0;
    pipelineLayoutCreateInfo.pSetLayouts = nullptr;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(_device, &pipelineLayoutCreateInfo, nullptr, &_trianglePipelineLayout) != VK_SUCCESS)
    {
        std::cerr << "Vulkan: Failed to create pipeline layout\n";
        return false;
    }

    return true;
}

void Engine::Unload()
{
    vkDeviceWaitIdle(_device);
    vkDestroyFence(_device, _renderFence, nullptr);
    vkDestroySemaphore(_device, _renderSemaphore, nullptr);
    vkDestroySemaphore(_device, _presentSemaphore, nullptr);

    vkDestroyShaderModule(_device, _simpleVertexShaderModule, nullptr);
    vkDestroyShaderModule(_device, _simpleFragmentShaderModule, nullptr);

    vkDestroyPipeline(_device, _trianglePipeline, nullptr);
    vkDestroyPipelineLayout(_device, _trianglePipelineLayout, nullptr);    

    vkDestroyCommandPool(_device, _commandPool, nullptr);

    for (size_t imageViewIndex = 0; imageViewIndex < _swapchainImageViews.size(); imageViewIndex++)
    {
        vkDestroyFramebuffer(_device, _framebuffers[imageViewIndex], nullptr);
        vkDestroyImageView(_device, _swapchainImageViews[imageViewIndex], nullptr);
    }

    vkDestroySwapchainKHR(_device, _swapchain, nullptr);
    vkDestroyRenderPass(_device, _renderPass, nullptr);

    vkDestroyDevice(_device, nullptr);
    vkDestroySurfaceKHR(_instance, _surface, nullptr);
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

bool Engine::TryLoadShaderModule(const std::string& filePath, VkShaderModule* shaderModule)
{
    auto buffer = ReadFile<uint32_t>(filePath);

    VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.pNext = nullptr;
    shaderModuleCreateInfo.codeSize = buffer.size();// * sizeof(uint32_t);
    shaderModuleCreateInfo.pCode = buffer.data();

    VkShaderModule shaderModuleTemp;
    if (vkCreateShaderModule(_device, &shaderModuleCreateInfo, nullptr, &shaderModuleTemp) != VK_SUCCESS)
    {
	    return false;
    }

    *shaderModule = shaderModuleTemp;
    return true;
}

GLFWwindow* Engine::GetWindow()
{
    return _window;
}

void Engine::SetDebugName(VkObjectType objectType, void* object, const std::string& debugName)
{
    const VkDebugUtilsObjectNameInfoEXT debugUtilsObjectNameInfo =
    {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .pNext = NULL,
        .objectType = objectType,
        .objectHandle = (uint64_t)object,
        .pObjectName = debugName.c_str(),
    };

    vkSetDebugUtilsObjectNameEXT(_device, &debugUtilsObjectNameInfo);
}
