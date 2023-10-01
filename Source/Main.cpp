#include <cstdint>
#include <cstdlib>

#include <iostream>
#include <string>
#include <vector>

#include "ApplicationIcon.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "VulkanBindings.hpp"
#include "Io.hpp"

constexpr int32_t MAX_FRAMES_IN_FLIGHT = 3;

struct ApplicationState
{
    GLFWwindow* MainWindow = nullptr;
    VulkanBindings Bindings;
    vkb::Instance Instance;
    VkSurfaceKHR Surface = nullptr;
    vkb::Device Device;
    vkb::Swapchain Swapchain;

    bool Vsync = true;
    VkClearValue ClearColor{ { { 0.15f, 0.0f, 0.6f, 1.0f } } };
};

struct RenderState
{
    VkQueue PresentQueue = nullptr;
    VkQueue GraphicsQueue = nullptr;

    VkRenderPass RenderPass = nullptr;

    std::vector<VkImage> SwapchainImages;
    std::vector<VkImageView> SwapchainImageViews;
    std::vector<VkFramebuffer> Framebuffers;

    VkPipelineLayout GraphicsPipelineLayout = nullptr;
    VkPipeline GraphicsPipeline = nullptr;

    VkCommandPool CommandPool = nullptr;
    std::vector<VkCommandBuffer> CommandBuffers;

    std::vector<VkSemaphore> AvailableSemaphores;
    std::vector<VkSemaphore> FinishedSemaphores;
    std::vector<VkFence> FencesInFlight;
    std::vector<VkFence> ImagesInFlight;
    size_t CurrentFrameIndex;    
};

static GLFWwindow* CreateMainWindow(const std::string& windowTitle)
{
    if (!glfwInit())
    {
        std::cout << "GLFW: Unable to initialize\n";
        return nullptr;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    auto primaryMonitor = glfwGetPrimaryMonitor();
    auto videoMode = glfwGetVideoMode(primaryMonitor);

    auto screenWidth = videoMode->width;
    auto screenHeight = videoMode->height;

    auto windowWidth = static_cast<int32_t>(screenWidth * 0.8f);
    auto windowHeight = static_cast<int32_t>(screenHeight * 0.8f);

    auto window = glfwCreateWindow(windowWidth, windowHeight, windowTitle.data(), nullptr, nullptr);
    if (window == nullptr)
    {
        std::cout << "GLFW: Unable to create window\n";
        glfwTerminate();
        return nullptr;
    }

    int32_t monitorLeft = 0;
    int32_t monitorTop = 0;
    glfwGetMonitorPos(primaryMonitor, &monitorLeft, &monitorTop);
    glfwSetWindowPos(window, screenWidth / 2 - windowWidth / 2 + monitorLeft, screenHeight / 2 - windowHeight / 2 + monitorTop);

    int32_t appImageWidth{};
    int32_t appImageHeight{};
    unsigned char* appImagePixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(&AppIcon[0]), AppIcon_length, &appImageWidth, &appImageHeight, nullptr, 4);
    if (appImagePixels != nullptr)
    {
        GLFWimage appImage{};
        appImage.width = 256;
        appImage.height = 256;
        appImage.pixels = appImagePixels;
        glfwSetWindowIcon(window, 1, &appImage);
        stbi_image_free(appImagePixels);
    }    

    return window;
}

static VkSurfaceKHR CreateSurface(ApplicationState& applicationState, VkAllocationCallbacks* allocator = nullptr)
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkResult result = glfwCreateWindowSurface(applicationState.Instance, applicationState.MainWindow, allocator, &surface);
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

static bool InitializeDevice(ApplicationState& applicationState)
{
    applicationState.MainWindow = CreateMainWindow("Fuk");
    if (applicationState.MainWindow == nullptr)
    {
        return false;
    }

    vkb::InstanceBuilder instanceBuilder;
    auto instanceResult = instanceBuilder
        .set_app_name("Fuk")
        .request_validation_layers()
        .enable_validation_layers()
        .use_default_debug_messenger()
        .build();
    if (!instanceResult)
    {
        std::cerr << "Vulkan: Failed to create instance.\nDetails: " << instanceResult.error().message() << "\n";
        return false;
    }

    applicationState.Instance = instanceResult.value();
    applicationState.Bindings.InitalizeFromInstance(applicationState.Instance);
    applicationState.Surface = CreateSurface(applicationState);

    vkb::PhysicalDeviceSelector physicalDeviceSelector{ applicationState.Instance };
    auto physicalDeviceSelectionResult = physicalDeviceSelector.set_surface(applicationState.Surface)
                        .set_minimum_version(1, 3)
                        .require_dedicated_transfer_queue()
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
    applicationState.Device = deviceBuilderResult.value();
    applicationState.Bindings.InitializeFromDevice(applicationState.Device);

    return true;    
}

bool CreateSwapchain(ApplicationState& applicationState)
{
    vkb::SwapchainBuilder swapchain_builder{ applicationState.Device };
    auto swapchainResult = swapchain_builder
        .set_old_swapchain(applicationState.Swapchain)
        .set_desired_present_mode(applicationState.Vsync ? VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR : VkPresentModeKHR::VK_PRESENT_MODE_IMMEDIATE_KHR)
        .build();
    if (!swapchainResult)
    {
        std::cout << swapchainResult.error().message() << " " << swapchainResult.vk_result() << "\n";
        return false;
    }
    vkb::destroy_swapchain(applicationState.Swapchain);
    applicationState.Swapchain = swapchainResult.value();
    return true;
}

VkShaderModule CreateShaderModule(ApplicationState& applicationState, const std::vector<char>& shaderCode)
{
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = shaderCode.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

    VkShaderModule shaderModule;
    if (applicationState.Bindings.vkCreateShaderModule(applicationState.Device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        return VK_NULL_HANDLE;
    }

    return shaderModule;
}

bool CreateRenderPass(ApplicationState& applicationState, RenderState& renderState)
{
    VkAttachmentDescription colorAttachmentDescription = {};
    colorAttachmentDescription.format = applicationState.Swapchain.image_format;
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

    VkSubpassDependency subpassDependency = {};
    subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependency.dstSubpass = 0;
    subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.srcAccessMask = 0;
    subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassCreateInfo = {};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.pAttachments = &colorAttachmentDescription;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpassDescription;
    renderPassCreateInfo.dependencyCount = 1;
    renderPassCreateInfo.pDependencies = &subpassDependency;

    if (applicationState.Bindings.vkCreateRenderPass(
        applicationState.Device,
        &renderPassCreateInfo,
        nullptr,
        &renderState.RenderPass) != VK_SUCCESS)
    {
        std::cout << "Vulkan: Failed to create render pass\n";
        return false;
    }

    return true;
}

bool CreateGraphicsPipeline(ApplicationState& applicationState, RenderState& renderState)
{
    auto vertexShaderCode = ReadFile("data/shaders/Simple.vs.spv");
    auto fragmentShaderCode = ReadFile("data/shaders/Simple.fs.spv");

    VkShaderModule vertexShaderModule = CreateShaderModule(applicationState, vertexShaderCode);
    VkShaderModule fragmentShaderModule = CreateShaderModule(applicationState, fragmentShaderCode);
    if (vertexShaderModule == VK_NULL_HANDLE || fragmentShaderModule == VK_NULL_HANDLE)
    {
        std::cout << "Vulkan: Failed to create shader module\n";
        return false;
    }

    VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
    vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexShaderStageCreateInfo.module = vertexShaderModule;
    vertexShaderStageCreateInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
    fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentShaderStageCreateInfo.module = fragmentShaderModule;
    fragmentShaderStageCreateInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shader_stages[] =
    {
        vertexShaderStageCreateInfo,
        fragmentShaderStageCreateInfo
    };

    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
    vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
    vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
    inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)applicationState.Swapchain.extent.width;
    viewport.height = (float)applicationState.Swapchain.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = { 0, 0 };
    scissor.extent = applicationState.Swapchain.extent;

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.pViewports = &viewport;
    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizerStateCreateInfo = {};
    rasterizerStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizerStateCreateInfo.depthClampEnable = VK_FALSE;
    rasterizerStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizerStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizerStateCreateInfo.lineWidth = 1.0f;
    rasterizerStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizerStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizerStateCreateInfo.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
    multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
    multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
    colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachmentState.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
    colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
    colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
    colorBlendStateCreateInfo.attachmentCount = 1;
    colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;
    colorBlendStateCreateInfo.blendConstants[0] = 0.0f;
    colorBlendStateCreateInfo.blendConstants[1] = 0.0f;
    colorBlendStateCreateInfo.blendConstants[2] = 0.0f;
    colorBlendStateCreateInfo.blendConstants[3] = 0.0f;

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 0;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;

    if (applicationState.Bindings.vkCreatePipelineLayout(
            applicationState.Device,
            &pipelineLayoutCreateInfo,
            nullptr,
            &renderState.GraphicsPipelineLayout) != VK_SUCCESS)
    {
        std::cout << "Vulkan: Failed to create pipeline layout\n";
        return false;
    }

    std::vector<VkDynamicState> dynamicStates =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicStatesCreateInfo = {};
    dynamicStatesCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStatesCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicStatesCreateInfo.pDynamicStates = dynamicStates.data();

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages = shader_stages;
    pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
    pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    pipelineCreateInfo.pRasterizationState = &rasterizerStateCreateInfo;
    pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
    pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
    pipelineCreateInfo.pDynamicState = &dynamicStatesCreateInfo;
    pipelineCreateInfo.layout = renderState.GraphicsPipelineLayout;
    pipelineCreateInfo.renderPass = renderState.RenderPass;
    pipelineCreateInfo.subpass = 0;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (applicationState.Bindings.vkCreateGraphicsPipelines(
            applicationState.Device,
            VK_NULL_HANDLE,
            1,
            &pipelineCreateInfo,
            nullptr,
            &renderState.GraphicsPipeline) != VK_SUCCESS)
    {
        std::cout << "Vulkan: Failed to create pipline\n";
        return false;
    }

    applicationState.Bindings.vkDestroyShaderModule(applicationState.Device, fragmentShaderModule, nullptr);
    applicationState.Bindings.vkDestroyShaderModule(applicationState.Device, vertexShaderModule, nullptr);

    return true;
}

bool InitializeQueues(ApplicationState& applicationState, RenderState& renderState)
{
    auto graphicsQueueResult = applicationState.Device.get_queue(vkb::QueueType::graphics);
    if (!graphicsQueueResult.has_value ())
    {
        std::cout << "Vulkan: Failed to get graphics queue\nDetails: " << graphicsQueueResult.error().message() << "\n";
        return false;
    }

    renderState.GraphicsQueue = graphicsQueueResult.value();

    auto presentQueueResult = applicationState.Device.get_queue(vkb::QueueType::present);
    if (!presentQueueResult.has_value())
    {
        std::cout << "Vulkan: Failed to get present queue\nDetails: " << presentQueueResult.error().message() << "\n";
        return false;
    }

    renderState.PresentQueue = presentQueueResult.value();
    return true;
}

bool CreateFramebuffers(ApplicationState& applicationState, RenderState& renderState)
{
    renderState.SwapchainImages = applicationState.Swapchain.get_images().value();
    renderState.SwapchainImageViews = applicationState.Swapchain.get_image_views().value();
    renderState.Framebuffers.resize(renderState.SwapchainImageViews.size());

    for (size_t i = 0; i < renderState.SwapchainImageViews.size(); i++)
    {
        VkImageView attachments[] = { renderState.SwapchainImageViews[i] };

        VkFramebufferCreateInfo framebuffer_info = {};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = renderState.RenderPass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = attachments;
        framebuffer_info.width = applicationState.Swapchain.extent.width;
        framebuffer_info.height = applicationState.Swapchain.extent.height;
        framebuffer_info.layers = 1;

        if (applicationState.Bindings.vkCreateFramebuffer(
            applicationState.Device,
            &framebuffer_info,
            nullptr,
            &renderState.Framebuffers[i]) != VK_SUCCESS)
        {
            return false;
        }
    }

    return true;
}

bool CreateCommandPool(ApplicationState& applicationState, RenderState& renderState)
{
    VkCommandPoolCreateInfo commandPoolCreateInfo = {};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.queueFamilyIndex = applicationState.Device.get_queue_index(vkb::QueueType::graphics).value();

    if (applicationState.Bindings.vkCreateCommandPool(
        applicationState.Device,
        &commandPoolCreateInfo,
        nullptr,
        &renderState.CommandPool) != VK_SUCCESS)
    {
        std::cout << "Vulkan: Failed to create command pool\n";
        return false;
    }

    return true;
}

static bool CreateCommandBuffers(ApplicationState& applicationState, RenderState& renderState)
{
    renderState.CommandBuffers.resize(renderState.Framebuffers.size());

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = renderState.CommandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = (uint32_t)renderState.CommandBuffers.size();

    if (applicationState.Bindings.vkAllocateCommandBuffers(
        applicationState.Device,
        &commandBufferAllocateInfo,
        renderState.CommandBuffers.data()) != VK_SUCCESS)
    {
        return false;
    }
    
    for (size_t i = 0; i < renderState.CommandBuffers.size (); i++)
    {
        VkCommandBufferBeginInfo commandBufferBeginInfo = {};
        commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (applicationState.Bindings.vkBeginCommandBuffer(renderState.CommandBuffers[i], &commandBufferBeginInfo) != VK_SUCCESS)
        {
            return false;
        }

        VkRenderPassBeginInfo renderPassBeginInfo = {};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = renderState.RenderPass;
        renderPassBeginInfo.framebuffer = renderState.Framebuffers[i];
        renderPassBeginInfo.renderArea.offset = { 0, 0 };
        renderPassBeginInfo.renderArea.extent = applicationState.Swapchain.extent;

        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = &applicationState.ClearColor;

        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)applicationState.Swapchain.extent.width;
        viewport.height = (float)applicationState.Swapchain.extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor = {};
        scissor.offset = { 0, 0 };
        scissor.extent = applicationState.Swapchain.extent;

        applicationState.Bindings.vkCmdSetViewport(renderState.CommandBuffers[i], 0, 1, &viewport);
        applicationState.Bindings.vkCmdSetScissor(renderState.CommandBuffers[i], 0, 1, &scissor);
        
        applicationState.Bindings.vkCmdBeginRenderPass(renderState.CommandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        applicationState.Bindings.vkCmdBindPipeline(renderState.CommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderState.GraphicsPipeline);
        applicationState.Bindings.vkCmdDraw(renderState.CommandBuffers[i], 3, 1, 0, 0);
        applicationState.Bindings.vkCmdEndRenderPass(renderState.CommandBuffers[i]);
        
        if (applicationState.Bindings.vkEndCommandBuffer(renderState.CommandBuffers[i]) != VK_SUCCESS)
        {
            std::cout << "Vulkan: Failed to record command buffer\n";
            return false;
        }
    }
    
    return true;
}

bool CreateSynchronizationObjects(ApplicationState& applicationState, RenderState& renderState)
{
    renderState.AvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderState.FinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderState.FencesInFlight.resize(MAX_FRAMES_IN_FLIGHT);
    renderState.ImagesInFlight.resize(applicationState.Swapchain.image_count, VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (applicationState.Bindings.vkCreateSemaphore(applicationState.Device, &semaphoreCreateInfo, nullptr, &renderState.AvailableSemaphores[i]) != VK_SUCCESS ||
            applicationState.Bindings.vkCreateSemaphore(applicationState.Device, &semaphoreCreateInfo, nullptr, &renderState.FinishedSemaphores[i]) != VK_SUCCESS ||
            applicationState.Bindings.vkCreateFence(applicationState.Device, &fenceCreateInfo, nullptr, &renderState.FencesInFlight[i]) != VK_SUCCESS)
        {
            std::cout << "Vulkan: Failed to create sync objects\n";
            return false;
        }
    }

    return true;
}

static bool RecreateSwapchain(ApplicationState& applicationState, RenderState& renderState)
{
    applicationState.Bindings.vkDeviceWaitIdle(applicationState.Device);
    applicationState.Bindings.vkDestroyCommandPool(applicationState.Device, renderState.CommandPool, nullptr);

    for (auto framebuffer : renderState.Framebuffers)
    {
        applicationState.Bindings.vkDestroyFramebuffer(applicationState.Device, framebuffer, nullptr);
    }

    applicationState.Swapchain.destroy_image_views(renderState.SwapchainImageViews);

    if (!CreateSwapchain(applicationState))
    {
        return false;
    }

    if (!CreateFramebuffers(applicationState, renderState))
    {
        return false;
    }

    if (!CreateCommandPool(applicationState, renderState))
    {
        return false;
    }

    if (!CreateCommandBuffers(applicationState, renderState))
    {
        return false;
    }

    return true;
}

static bool DrawFrame(ApplicationState& applicationState, RenderState& renderState)
{
    applicationState.Bindings.vkWaitForFences(applicationState.Device, 1, &renderState.FencesInFlight[renderState.CurrentFrameIndex], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex = 0;
    VkResult nextImageResult = applicationState.Bindings.vkAcquireNextImageKHR(
        applicationState.Device,
        applicationState.Swapchain,
        UINT64_MAX,
        renderState.AvailableSemaphores[renderState.CurrentFrameIndex],
        VK_NULL_HANDLE,
        &imageIndex);

    if (nextImageResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return RecreateSwapchain(applicationState, renderState);
    }
    else if (nextImageResult != VK_SUCCESS && nextImageResult != VK_SUBOPTIMAL_KHR)
    {
        std::cout << "Vulkan: Failed to acquire swapchain image.\nDetails: " << nextImageResult << "\n";
        return false;
    }

    if (renderState.ImagesInFlight[imageIndex] != VK_NULL_HANDLE)
    {
        applicationState.Bindings.vkWaitForFences (applicationState.Device, 1, &renderState.ImagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    renderState.ImagesInFlight[imageIndex] = renderState.FencesInFlight[renderState.CurrentFrameIndex];

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] =
    {
        renderState.AvailableSemaphores[renderState.CurrentFrameIndex]
    };

    VkPipelineStageFlags waitStages[] =
    {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &renderState.CommandBuffers[imageIndex];

    VkSemaphore signalSemaphores[] =
    {
        renderState.FinishedSemaphores[renderState.CurrentFrameIndex]
    };

    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    applicationState.Bindings.vkResetFences(applicationState.Device, 1, &renderState.FencesInFlight[renderState.CurrentFrameIndex]);

    if (applicationState.Bindings.vkQueueSubmit(
        renderState.GraphicsQueue,
        1,
        &submitInfo,
        renderState.FencesInFlight[renderState.CurrentFrameIndex]) != VK_SUCCESS)
    {
        std::cout << "Vulkan: Failed to submit draw command buffer\n";
        return false;
    }

    VkSwapchainKHR swapChains[] =
    {
        applicationState.Swapchain
    };

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    auto presentResult = applicationState.Bindings.vkQueuePresentKHR(renderState.PresentQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
    {
        return RecreateSwapchain(applicationState, renderState);
    }
    else if (presentResult != VK_SUCCESS)
    {
        std::cout << "Vulkan: Failed to present swapchain image\n";
        return false;
    }

    renderState.CurrentFrameIndex = (renderState.CurrentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    return true;
}

void Cleanup(ApplicationState& applicationState, RenderState& renderState)
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        applicationState.Bindings.vkDestroySemaphore(applicationState.Device, renderState.FinishedSemaphores[i], nullptr);
        applicationState.Bindings.vkDestroySemaphore(applicationState.Device, renderState.AvailableSemaphores[i], nullptr);
        applicationState.Bindings.vkDestroyFence(applicationState.Device, renderState.FencesInFlight[i], nullptr);
    }

    applicationState.Bindings.vkDestroyCommandPool(applicationState.Device, renderState.CommandPool, nullptr);

    for (auto framebuffer : renderState.Framebuffers)
    {
        applicationState.Bindings.vkDestroyFramebuffer(applicationState.Device, framebuffer, nullptr);
    }

    applicationState.Bindings.vkDestroyPipeline(applicationState.Device, renderState.GraphicsPipeline, nullptr);
    applicationState.Bindings.vkDestroyPipelineLayout(applicationState.Device, renderState.GraphicsPipelineLayout, nullptr);
    applicationState.Bindings.vkDestroyRenderPass(applicationState.Device, renderState.RenderPass, nullptr);

    applicationState.Swapchain.destroy_image_views(renderState.SwapchainImageViews);

    vkb::destroy_swapchain(applicationState.Swapchain);
    vkb::destroy_device(applicationState.Device);
    vkb::destroy_surface(applicationState.Instance, applicationState.Surface);
    vkb::destroy_instance(applicationState.Instance);
}

int32_t main([[maybe_unused]] int32_t argc, [[maybe_unused]] char* argv[])
{
    ApplicationState applicationState;
    RenderState renderState;

    if (!InitializeDevice(applicationState))
    {
        return EXIT_FAILURE;
    }

    if (!CreateSwapchain(applicationState))
    {
        return EXIT_FAILURE;
    }

    if (!InitializeQueues(applicationState, renderState))
    {
        return EXIT_FAILURE;
    }

    if (!CreateRenderPass(applicationState, renderState))
    {
        return EXIT_FAILURE;
    }
    
    if (!CreateGraphicsPipeline(applicationState, renderState))
    {
        return EXIT_FAILURE;
    }

    if (!CreateFramebuffers(applicationState, renderState))
    {
        return EXIT_FAILURE;
    }
    
    if (!CreateCommandPool(applicationState, renderState))
    {
        return EXIT_FAILURE;
    }

    if (!CreateCommandBuffers(applicationState, renderState))
    {
        return EXIT_FAILURE;
    }

    if (!CreateSynchronizationObjects(applicationState, renderState))
    {
        return EXIT_FAILURE;
    }

    auto currentTime = glfwGetTime();
    auto previousTime = currentTime;

    while (!glfwWindowShouldClose(applicationState.MainWindow))
    {
        glfwPollEvents();
        currentTime = glfwGetTime();
        auto deltaTime = static_cast<float>(currentTime - previousTime);
        previousTime = currentTime;

		if (!DrawFrame(applicationState, renderState))
        {
			std::cout << "Fuk: Failed to draw frame\n";
			break;
		}
    }

    applicationState.Bindings.vkDeviceWaitIdle(applicationState.Device);

    Cleanup(applicationState, renderState);

    glfwDestroyWindow(applicationState.MainWindow);
    glfwTerminate();

    return EXIT_SUCCESS;
}