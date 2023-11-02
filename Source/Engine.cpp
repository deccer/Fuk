#include "Engine.hpp"
#include "ApplicationIcon.hpp"
#include "Io.hpp"
#include "Stbi.hpp"
#include "PipelineBuilder.hpp"

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

std::vector<VertexPositionNormalUv> ConvertVertexBufferFormat(const fastgltf::Asset& model, const fastgltf::Primitive& primitive)
{
    std::vector<glm::vec3> positions;
    auto& positionAccessor = model.accessors[primitive.findAttribute("POSITION")->second];
    positions.resize(positionAccessor.count);
    fastgltf::iterateAccessorWithIndex<glm::vec3>(model,
                                                  positionAccessor,
                                                  [&](glm::vec3 position, std::size_t idx) { positions[idx] = position; });

    std::vector<glm::vec3> normals;
    auto& normalAccessor = model.accessors[primitive.findAttribute("NORMAL")->second];
    normals.resize(normalAccessor.count);
    fastgltf::iterateAccessorWithIndex<glm::vec3>(model,
                                                  normalAccessor,
                                                  [&](glm::vec3 normal, std::size_t idx) { normals[idx] = normal; });

    std::vector<glm::vec2> texcoords;

    // Textureless meshes will use factors instead of textures
    if (primitive.findAttribute("TEXCOORD_0") != primitive.attributes.end())
    {
        auto& texcoordAccessor = model.accessors[primitive.findAttribute("TEXCOORD_0")->second];
        texcoords.resize(texcoordAccessor.count);
        fastgltf::iterateAccessorWithIndex<glm::vec2>(model,
                                                    texcoordAccessor,
                                                    [&](glm::vec2 texcoord, std::size_t idx)
                                                    { texcoords[idx] = texcoord; });
    }
    else
    {
        // If no texcoord attribute, fill with empty texcoords to keep everything consistent and happy
        texcoords.resize(positions.size(), {});
    }

    std::vector<VertexPositionNormalUv> vertices;
    vertices.resize(positions.size());

    for (size_t i = 0; i < positions.size(); i++)
    {
        vertices[i] = 
        {
            positions[i],
            normals[i],
            texcoords[i]
        };
    }

    return vertices;
}

std::vector<uint32_t> ConvertIndexBufferFormat(
    const fastgltf::Asset& model,
    const fastgltf::Primitive& primitive)
{
    auto indices = std::vector<uint32_t>();
    auto& accessor = model.accessors[primitive.indicesAccessor.value()];
    indices.resize(accessor.count);
    fastgltf::iterateAccessorWithIndex<uint32_t>(model, accessor, [&](uint32_t index, size_t idx)
    {
        indices[idx] = index;
    });
    return indices;
}

std::expected<AllocatedImage, std::string> Engine::CreateImage(
    const std::string& label,
    VkFormat format,
    VkImageUsageFlags imageUsageFlags,
    VkImageAspectFlags imageAspectFlags,
    VkExtent3D extent)
{
    AllocatedImage image;
    if (vmaCreateImage(
        _allocator,
        ToTempPtr(VkImageCreateInfo
        {
            .sType = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .imageType = VkImageType::VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = extent,
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
            .tiling = VkImageTiling::VK_IMAGE_TILING_OPTIMAL,
            .usage = imageUsageFlags
        }),
        ToTempPtr(VmaAllocationCreateInfo
        {
            .usage = VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        }),
        &image.image,
        &image.allocation,
        nullptr) != VK_SUCCESS)
    {
        return std::unexpected("Vulkan: Unable to create image");
    }

    SetDebugName(_device, image.image, label);

    if (vkCreateImageView(
        _device,
        ToTempPtr(VkImageViewCreateInfo
        {
            .sType = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .image = image.image,            
            .viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .subresourceRange = VkImageSubresourceRange
            {
                .aspectMask = imageAspectFlags,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            }
        }),
        nullptr,
        &image.imageView) != VK_SUCCESS)
    {
        return std::unexpected("Vulkan: Failed to create image view");
    }

    auto viewLabel = std::format("{}_ImageView", label);
    SetDebugName(_device, image.imageView, viewLabel);

    _deletionQueue.Push([=, this]()
    {
        vkDestroyImageView(_device, image.imageView, nullptr);
        vmaDestroyImage(_allocator, image.image, image.allocation);
    });

    return image;
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

    VkExtent3D depthImageExtent =
    {
        .width = _windowExtent.width,
        .height = _windowExtent.height,
        .depth = 1
    };

    auto depthImageResult = CreateImage(
        "DepthImage",
        _depthFormat,
        VkImageUsageFlagBits::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT,
        depthImageExtent);
    if (!depthImageResult.has_value())
    {
        std::cerr << depthImageResult.error();
        return false;
    }

    _depthImage = depthImageResult.value();

    if (!InitializeFramebuffers())
    {
        return false;
    }

    if (!InitializeDescriptors())
    {
        return false;
    }

    if (!InitializeSynchronizationStructures())
    {
        return false;
    }

    return true;
}

bool Engine::Load()
{
    auto loadShaderModuleResult = LoadShaderModule("data/shaders/Simple.vs.glsl.spv");
    if (!loadShaderModuleResult.has_value())
    {
        std::cout << loadShaderModuleResult.error();
        return false;
    }

    _simpleVertexShaderModule = loadShaderModuleResult.value();

    loadShaderModuleResult = LoadShaderModule("data/shaders/Simple.fs.glsl.spv");
    if (!loadShaderModuleResult.has_value())
    {
        std::cout << loadShaderModuleResult.error();
        return false;
    }

    _simpleFragmentShaderModule = loadShaderModuleResult.value();

    VkViewport viewport =
    {
        .x = 0,
        .y = (float)_windowExtent.height,
        .width = (float)_windowExtent.width,
        .height = -(float)_windowExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    VkRect2D scissor =
    {
        .offset = { 0, 0 },
        .extent = _windowExtent
    };

    PipelineBuilder pipelineBuilder(_deletionQueue);
    auto pipelineResult = pipelineBuilder
        .WithGraphicsShadingStages(_simpleVertexShaderModule, _simpleFragmentShaderModule)
        .WithVertexInput(VertexPositionNormalUv::GetVertexInputDescription())
        .WithTopology(VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .WithPolygonMode(VkPolygonMode::VK_POLYGON_MODE_FILL)
        .WithViewportAndScissor(viewport, scissor)     
        .WithDepthTestingEnabled(VkCompareOp::VK_COMPARE_OP_LESS)
        .WithoutBlending()
        .WithoutMultisampling()
        .WithDescriptorSetLayout(_globalDescriptorSetLayout)
        .WithDescriptorSetLayout(_objectDescriptorSetLayout)
        .Build("OpaquePipeline", _device, _renderPass);
    if (!pipelineResult.has_value())
    {
        std::cout << pipelineResult.error();
        return false;
    }

    if (!LoadMeshFromFile("SM_Cubes", "data/models/deccer-cubes/SM_Deccer_Cubes_Textured_Complex.gltf"))
    {
        return false;
    }

    auto meshes = GetModel("SM_Cubes");
    auto meshIndex = 0;
    auto originTransform = glm::translate(glm::mat4(1.0f), glm::vec3(-20.0f, 0.0f, 0.0f));
    for (size_t i = 0; i < 3; i++)
    {
        auto rootTransform = glm::translate(originTransform, glm::vec3(meshIndex * 10.0f, 0.0f, 0.0f));    
        for (auto& mesh : meshes)
        {
            Renderable renderable;
            renderable.pipeline = pipelineResult.value();
            renderable.mesh = mesh;
            renderable.worldMatrix = rootTransform * mesh->worldMatrix;
            _renderables.push_back(renderable);
        }
        meshIndex++;        
    }

    return true;
}

bool Engine::LoadMeshFromFile(const std::string& modelName, const std::string& filePath)
{
    fastgltf::Parser parser(fastgltf::Extensions::KHR_mesh_quantization);

    auto path = std::filesystem::path{filePath};

    constexpr auto gltfOptions =
        fastgltf::Options::DontRequireValidAssetMember |
        fastgltf::Options::AllowDouble |
        fastgltf::Options::LoadGLBBuffers |
        fastgltf::Options::LoadExternalBuffers |
        fastgltf::Options::LoadExternalImages;

    fastgltf::GltfDataBuffer data;
    data.loadFromFile(path);

    auto gltfType = fastgltf::determineGltfFileType(&data);
    fastgltf::Expected<fastgltf::Asset> assetResult(fastgltf::Error::None);
    if (gltfType == fastgltf::GltfType::glTF)
    {
        assetResult = parser.loadGLTF(&data, path.parent_path(), gltfOptions);
    } 
    else if (gltfType == fastgltf::GltfType::GLB)
    {
        assetResult = parser.loadBinaryGLTF(&data, path.parent_path(), gltfOptions);
    }
    else
    {
        std::cerr << "fastgltf: Failed to determine glTF container\n";
        return false;
    }

    if (assetResult.error() != fastgltf::Error::None)
    {
        std::cerr << "fastgltf: Failed to load glTF: " << fastgltf::getErrorMessage(assetResult.error()) << "\n";
        return false;
    }

    auto& asset = assetResult.get();

    std::stack<std::pair<const fastgltf::Node*, glm::mat4>> nodeStack;
    glm::mat4 rootTransform = glm::mat4(1.0f);

    for (auto nodeIndex : asset.scenes[0].nodeIndices)
    {
        nodeStack.emplace(&asset.nodes[nodeIndex], rootTransform);
    }

    std::vector<std::string> meshNames;
    
    while (!nodeStack.empty())
    {
        decltype(nodeStack)::value_type top = nodeStack.top();
        const auto& [node, parentGlobalTransform] = top;
        nodeStack.pop();

        glm::mat4 localTransform = NodeToMat4(*node);
        glm::mat4 globalTransform = parentGlobalTransform * localTransform;

        for (auto childNodeIndex : node->children)
        {
            nodeStack.emplace(&asset.nodes[childNodeIndex], globalTransform);
        }

        if (node->meshIndex.has_value())
        {
            for (const fastgltf::Mesh& fgMesh = asset.meshes[node->meshIndex.value()];
                 const auto& primitive : fgMesh.primitives)
            {
                Mesh mesh;
                mesh.vertices = ConvertVertexBufferFormat(asset, primitive);
                mesh.indices = ConvertIndexBufferFormat(asset, primitive);
                mesh.worldMatrix = globalTransform;
                mesh.name = fgMesh.name;
                
                auto createStagingBufferResult = CreateStagingBuffer(std::span(mesh.vertices));
                if (!createStagingBufferResult.has_value())
                {
                    std::cerr << createStagingBufferResult.error() << "\n";
                    return false;
                }

                auto bufferSize = createStagingBufferResult.value().bufferSize;
                auto createBufferResult = CreateBuffer<VertexPositionNormalUv>(
                    std::format("VertexBuffer_{}", node->name),
                    bufferSize,
                    VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY);
                if (!createBufferResult.has_value())
                {
                    std::cerr << createBufferResult.error() << "\n";
                    return false;
                }

                mesh.vertexBuffer = createBufferResult.value();

                SubmitImmediately([=](VkCommandBuffer commandBuffer)
                {
                    vkCmdCopyBuffer(commandBuffer, createStagingBufferResult.value().buffer, mesh.vertexBuffer.buffer, 1, ToTempPtr(VkBufferCopy
                    {
                        .srcOffset = 0,
                        .dstOffset = 0,
                        .size = bufferSize
                    }));
                });

                createBufferResult = CreateBuffer(
                    std::format("IndexBuffer_{}", node->name),
                    VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU,
                    std::span(mesh.indices));
                if (!createBufferResult.has_value())
                {
                    std::cerr << createBufferResult.error() << "\n";
                    return false;
                }

                mesh.indexBuffer = createBufferResult.value();

                auto meshName = node->name.c_str();
                meshNames.push_back(meshName);
                _meshNameToMeshMap.emplace(meshName, mesh);
            }

        }
    }

    _modelNameToMeshNameMap.emplace(modelName, meshNames);

    return true;
}

bool Engine::Draw()
{
    FrameData frameData = GetCurrentFrameData();

    VkResult result = vkWaitForFences(_device, 1, &frameData.renderFence, true, 1000000000);
    if (result != VK_SUCCESS)
    {
        std::cerr << "Vulkan: Unable to wait for render fence\n" << result << "\n";
        return false;
    }

    if (vkResetFences(_device, 1, &frameData.renderFence) != VK_SUCCESS)
    {
        std::cerr << "Vulkan: Unable to reset render fence\n";
        return false;
    }

    uint32_t swapchainImageIndex;
    if (vkAcquireNextImageKHR(
        _device,
        _swapchain,
        1000000000,
        frameData.presentSemaphore,
        nullptr,
        &swapchainImageIndex) != VK_SUCCESS)
    {
        std::cerr << "Vulkan: Unable to acquire next image\n";
        return false;
    }

    if (vkResetCommandBuffer(frameData.commandBuffer, 0) != VK_SUCCESS)
    {
        std::cerr << "Vulkan: Unable to reset command buffer\n";
        return false;
    }

    VkCommandBufferBeginInfo commandBufferBeginInfo = {};
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.pNext = nullptr;
    commandBufferBeginInfo.pInheritanceInfo = nullptr;
    commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(frameData.commandBuffer, &commandBufferBeginInfo) != VK_SUCCESS)
    {
        std::cerr << "Vulkan: Failed to begin command buffer\n";
        return false;
    }

    VkClearValue clearValue;
    float flash = abs(sin(_frameIndex / 120.f));
    clearValue.color = { { 0.0f, 0.0f, flash, 1.0f } };

    VkClearValue clearDepthValue;
    clearDepthValue.depthStencil.depth = 1;

    VkRenderPassBeginInfo renderPassBeginInfo = {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.pNext = nullptr;

    renderPassBeginInfo.renderPass = _renderPass;
    renderPassBeginInfo.renderArea.offset.x = 0;
    renderPassBeginInfo.renderArea.offset.y = 0;
    renderPassBeginInfo.renderArea.extent = _windowExtent;
    renderPassBeginInfo.framebuffer = _framebuffers[swapchainImageIndex];

    VkClearValue clearValues[2] = { clearValue, clearDepthValue };
    renderPassBeginInfo.clearValueCount = 2;
    renderPassBeginInfo.pClearValues = &clearValues[0];

    vkCmdBeginRenderPass(frameData.commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);    

    DrawRenderables(frameData.commandBuffer, _renderables.data(), _renderables.size());

    vkCmdEndRenderPass(frameData.commandBuffer);

    if (vkEndCommandBuffer(frameData.commandBuffer) != VK_SUCCESS)
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
    submitInfo.pWaitSemaphores = &frameData.presentSemaphore;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &frameData.renderSemaphore;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frameData.commandBuffer;

    if (vkQueueSubmit(_graphicsQueue, 1, &submitInfo, frameData.renderFence) != VK_SUCCESS)
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
    presentInfo.pWaitSemaphores = &frameData.renderSemaphore;
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

    SetDebugName(_device, _swapchain, "SwapChain");

    _deletionQueue.Push([=, this]()
    {
        vkDestroySwapchainKHR(_device, _swapchain, nullptr);
    });

    return true;
}

bool Engine::InitializeCommandBuffers()
{
    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        if (vkCreateCommandPool(
            _device,
            ToTempPtr(VkCommandPoolCreateInfo
            {
                .sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .pNext = nullptr,
                .flags = VkCommandPoolCreateFlagBits::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = _graphicsQueueFamily,                
            }),
            nullptr,
            &_frameDates[i].commandPool) != VK_SUCCESS)
        {
            std::cout << "Vulkan: Failed to create command pool\n";
            return false;
        }

        SetDebugName(_device, _frameDates[i].commandPool, std::format("CommandPool_{}\0", i));

        if(vkAllocateCommandBuffers(
            _device,
            ToTempPtr(VkCommandBufferAllocateInfo
            {
                .sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext = nullptr,
                .commandPool = _frameDates[i].commandPool,
                .level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            }),
            &_frameDates[i].commandBuffer) != VK_SUCCESS)
        {
            std::cout << "Vulkan: Failed to create command buffer\n";
            return false;
        }

        SetDebugName(_device, _frameDates[i].commandBuffer, std::format("CommandBuffer_{}", i));

        _deletionQueue.Push([=, this]()
        {
            vkDestroyCommandPool(_device, _frameDates[i].commandPool, nullptr);
        });
    }

    if (vkCreateCommandPool(
        _device,
        ToTempPtr(VkCommandPoolCreateInfo
        {
                .sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .pNext = nullptr,
                .flags = VkCommandPoolCreateFlagBits::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = _graphicsQueueFamily,                
        }),
        nullptr,
        &_uploadContext.commandPool) != VK_SUCCESS)
    {
        return false;
    }

    _deletionQueue.Push([=, this]()
    {
        vkDestroyCommandPool(_device, _uploadContext.commandPool, nullptr);
    });

    if (vkAllocateCommandBuffers(
        _device,
        ToTempPtr(VkCommandBufferAllocateInfo
        {
            .sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = _uploadContext.commandPool,
            .level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        }),
        &_uploadContext.commandBuffer) != VK_SUCCESS)
    {
        return false;
    }

    return true;
}

bool Engine::InitializeRenderPass()
{
    _depthFormat = VkFormat::VK_FORMAT_D32_SFLOAT;

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

    VkAttachmentDescription depthAttachmentDescription = {};
    depthAttachmentDescription.flags = 0;
    depthAttachmentDescription.format = _depthFormat;
    depthAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentReference = {};
    depthAttachmentReference.attachment = 1;
    depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorAttachmentReference;
    subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;

    VkAttachmentDescription attachments[2] =
    {
        colorAttachmentDescription, 
        depthAttachmentDescription
    };

    VkSubpassDependency colorDependency = {};
    colorDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    colorDependency.dstSubpass = 0;
    colorDependency.srcStageMask = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    colorDependency.srcAccessMask = 0;
    colorDependency.dstStageMask = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    colorDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkSubpassDependency depthDependency = {};
    depthDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    depthDependency.dstSubpass = 0;
    depthDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depthDependency.srcAccessMask = 0;
    depthDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkSubpassDependency subpassDependencies[] =
    { 
        colorDependency,
        depthDependency
    };

    VkRenderPassCreateInfo renderPassCreateInfo = {};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = 2;
    renderPassCreateInfo.pAttachments = &attachments[0];
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpassDescription;
    renderPassCreateInfo.dependencyCount = 2;
    renderPassCreateInfo.pDependencies = subpassDependencies;

    if (vkCreateRenderPass(_device, &renderPassCreateInfo, nullptr, &_renderPass) != VK_SUCCESS)
    {
        std::cerr << "Vulkan: Failed to create default render pass\n";
        return false;
    }

    SetDebugName(_device, _renderPass, "RenderPass");

    _deletionQueue.Push([=, this]()
    {
        vkDestroyRenderPass(_device, _renderPass, nullptr);
    });

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
    framebufferCreateInfo.width = _windowExtent.width;
    framebufferCreateInfo.height = _windowExtent.height;
    framebufferCreateInfo.layers = 1;

    auto swapchainImageCount = _swapchainImages.size();
    _framebuffers = std::vector<VkFramebuffer>(swapchainImageCount);

    for (size_t i = 0; i < swapchainImageCount; i++)
    {
        VkImageView attachments[2];
        attachments[0] = _swapchainImageViews[i];
        attachments[1] = _depthImage.imageView;

        framebufferCreateInfo.attachmentCount = 2;
        framebufferCreateInfo.pAttachments = attachments;

        if(vkCreateFramebuffer(_device, &framebufferCreateInfo, nullptr, &_framebuffers[i]) != VK_SUCCESS)
        {
            return false;
        }
    }

    return true;
}

bool Engine::InitializeDescriptors()
{
    std::vector<VkDescriptorPoolSize> descriptorPoolSizes =
    {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 16 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 16 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16 }
    };

    if (vkCreateDescriptorPool(
        _device,
        ToTempPtr(VkDescriptorPoolCreateInfo
        {
            .sType = VkStructureType::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .maxSets = 16,
            .poolSizeCount = (uint32_t)descriptorPoolSizes.size(),
            .pPoolSizes = descriptorPoolSizes.data()
        }),
        nullptr,
        &_descriptorPool) != VK_SUCCESS)
    {
        std::cerr << "Vulkan: Failed to create descriptor pool\n";
        return false;
    }

    VkDescriptorSetLayoutBinding gpuCameraDataBufferBinding = {};
    gpuCameraDataBufferBinding.binding = 0;
    gpuCameraDataBufferBinding.descriptorCount = 1;
    gpuCameraDataBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    gpuCameraDataBufferBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding gpuSceneDataBufferBinding = {};
    gpuSceneDataBufferBinding.binding = 1;
    gpuSceneDataBufferBinding.descriptorCount = 1;
    gpuSceneDataBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    gpuSceneDataBufferBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding globalDescriptorSetLayoutBindings[] =
    {
        gpuCameraDataBufferBinding,
        gpuSceneDataBufferBinding
    };

    VkDescriptorSetLayoutCreateInfo globalDescriptorSetLayoutCreateInfo = {};
    globalDescriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    globalDescriptorSetLayoutCreateInfo.pNext = nullptr;
    globalDescriptorSetLayoutCreateInfo.bindingCount = 2;
    globalDescriptorSetLayoutCreateInfo.flags = 0;
    globalDescriptorSetLayoutCreateInfo.pBindings = globalDescriptorSetLayoutBindings;

    if (vkCreateDescriptorSetLayout(
        _device,
        &globalDescriptorSetLayoutCreateInfo,
        nullptr,
        &_globalDescriptorSetLayout) != VK_SUCCESS)
    {
        std::cerr << "Vulkan: Failed to create descriptor set layout\n";
        return false;
    }

    SetDebugName(_device, _globalDescriptorSetLayout, "GlobalDescriptorSetLayout");

    VkDescriptorSetLayoutBinding objectDescriptorSetLayoutBinding = {};
    objectDescriptorSetLayoutBinding.binding = 0;
    objectDescriptorSetLayoutBinding.descriptorCount = 1;
    objectDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    objectDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo objectDescriptorSetLayoutCreateInfo = {};
    objectDescriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    objectDescriptorSetLayoutCreateInfo.pNext = nullptr;
    objectDescriptorSetLayoutCreateInfo.bindingCount = 1;
    objectDescriptorSetLayoutCreateInfo.flags = 0;
    objectDescriptorSetLayoutCreateInfo.pBindings = &objectDescriptorSetLayoutBinding;

    if (vkCreateDescriptorSetLayout(
        _device,
        &objectDescriptorSetLayoutCreateInfo,
        nullptr,
        &_objectDescriptorSetLayout) != VK_SUCCESS)
    {
        std::cerr << "Vulkan: Failed to create descriptor set layout\n";
        return false;
    }

    SetDebugName(_device, _objectDescriptorSetLayout, "ObjectDescriptorSetLayout");    

    _deletionQueue.Push([&]()
    {
        vkDestroyDescriptorSetLayout(_device, _globalDescriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(_device, _objectDescriptorSetLayout, nullptr);
        vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
    });

    const size_t gpuSceneDataBufferSize = FRAMES_IN_FLIGHT * PadUniformBufferSize(sizeof(GpuSceneData));
    auto gpuSceneDataBufferResult = CreateBuffer<GpuSceneData>(
        "GpuSceneData",
        gpuSceneDataBufferSize,
        VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU);
    if (!gpuSceneDataBufferResult.has_value())
    {
        std::cerr << gpuSceneDataBufferResult.error() << "\n";
        return false;
    }

    _gpuSceneDataBuffer = gpuSceneDataBufferResult.value();

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        std::string label = std::format("GpuCameraData_{}", i);
        auto createBufferResult = CreateBuffer<GpuCameraData>(
            label,
            VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU);
        if (!createBufferResult.has_value())
        {
            return false;
        }

        _frameDates[i].cameraBuffer = createBufferResult.value();

        size_t dataSize = 100 * sizeof(GpuObjectData);
        label = std::format("GpuObjectData_{}", i);
        createBufferResult = CreateBuffer<GpuObjectData>(
            label,
            dataSize,
            VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU);
        if (!createBufferResult.has_value())
        {
            return false;
        }

        _frameDates[i].objectBuffer = createBufferResult.value();

        if (vkAllocateDescriptorSets(
            _device,
            ToTempPtr(VkDescriptorSetAllocateInfo
            {
                .sType = VkStructureType::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .pNext = nullptr,
                .descriptorPool = _descriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts = &_globalDescriptorSetLayout
            }),
            &_frameDates[i].globalDescriptorSet) != VK_SUCCESS)
        {
            std::cerr << "Vulkan: Failed to allocate global descriptor set\n";
            return false;
        }

        SetDebugName(_device, _frameDates[i].globalDescriptorSet, "GlobalDescriptorSet");

        if (vkAllocateDescriptorSets(
            _device,
            ToTempPtr(VkDescriptorSetAllocateInfo
            {
                .sType = VkStructureType::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .pNext = nullptr,
                .descriptorPool = _descriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts = &_objectDescriptorSetLayout
            }),
            &_frameDates[i].objectDescriptorSet) != VK_SUCCESS)
        {
            std::cerr << "Vulkan: Failed to allocate object descriptor set\n";
            return false;
        }

        SetDebugName(_device, _frameDates[i].objectDescriptorSet, "ObjectDescriptorSet");

        VkDescriptorBufferInfo gpuCameraDataDescriptorBufferInfo = {};
        gpuCameraDataDescriptorBufferInfo.buffer = _frameDates[i].cameraBuffer.buffer;
        gpuCameraDataDescriptorBufferInfo.offset = 0;
        gpuCameraDataDescriptorBufferInfo.range = sizeof(GpuCameraData);

        VkWriteDescriptorSet gpuCameraDataWriteDescriptorSet = {};
        gpuCameraDataWriteDescriptorSet.sType = VkStructureType::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        gpuCameraDataWriteDescriptorSet.pNext = nullptr;
        gpuCameraDataWriteDescriptorSet.dstBinding = 0;
        gpuCameraDataWriteDescriptorSet.dstSet = _frameDates[i].globalDescriptorSet;
        gpuCameraDataWriteDescriptorSet.descriptorCount = 1;
        gpuCameraDataWriteDescriptorSet.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        gpuCameraDataWriteDescriptorSet.pBufferInfo = &gpuCameraDataDescriptorBufferInfo;

        VkDescriptorBufferInfo gpuSceneDataDescriptorBufferInfo = {};
        gpuSceneDataDescriptorBufferInfo.buffer = _gpuSceneDataBuffer.buffer;
        gpuSceneDataDescriptorBufferInfo.offset = 0;
        gpuSceneDataDescriptorBufferInfo.range = sizeof(GpuSceneData);

        VkWriteDescriptorSet gpuSceneDataWriteDescriptorSet = {};
        gpuSceneDataWriteDescriptorSet.sType = VkStructureType::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        gpuSceneDataWriteDescriptorSet.pNext = nullptr;
        gpuSceneDataWriteDescriptorSet.dstBinding = 1;
        gpuSceneDataWriteDescriptorSet.dstSet = _frameDates[i].globalDescriptorSet;
        gpuSceneDataWriteDescriptorSet.descriptorCount = 1;
        gpuSceneDataWriteDescriptorSet.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        gpuSceneDataWriteDescriptorSet.pBufferInfo = &gpuSceneDataDescriptorBufferInfo;

        VkDescriptorBufferInfo gpuObjectDataDescriptorBufferInfo = {};
        gpuObjectDataDescriptorBufferInfo.buffer = _frameDates[i].objectBuffer.buffer;
        gpuObjectDataDescriptorBufferInfo.offset = 0;
        gpuObjectDataDescriptorBufferInfo.range = 100 * sizeof(GpuObjectData);

        VkWriteDescriptorSet gpuObjectDataWriteDescriptorSet = {};
        gpuObjectDataWriteDescriptorSet.sType = VkStructureType::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        gpuObjectDataWriteDescriptorSet.pNext = nullptr;
        gpuObjectDataWriteDescriptorSet.dstBinding = 0;
        gpuObjectDataWriteDescriptorSet.dstSet = _frameDates[i].objectDescriptorSet;
        gpuObjectDataWriteDescriptorSet.descriptorCount = 1;
        gpuObjectDataWriteDescriptorSet.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        gpuObjectDataWriteDescriptorSet.pBufferInfo = &gpuObjectDataDescriptorBufferInfo;

        VkWriteDescriptorSet writeDescriptorSets[] =
        {
            gpuCameraDataWriteDescriptorSet,
            gpuSceneDataWriteDescriptorSet,
            gpuObjectDataWriteDescriptorSet
        };

        vkUpdateDescriptorSets(
            _device,
            3,
            writeDescriptorSets,
            0,
            nullptr);
    }

    return true;
}

bool Engine::InitializeSynchronizationStructures()
{
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = nullptr;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = nullptr;
    semaphoreCreateInfo.flags = 0;

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        if (vkCreateFence(
            _device,
            &fenceCreateInfo,
            nullptr,
            &_frameDates[i].renderFence) != VK_SUCCESS)
        {
            std::cerr << "Vulkan: Failed to create render fence\n";
            return false;
        }

        if (vkCreateSemaphore(
            _device,
            &semaphoreCreateInfo,
            nullptr,
            &_frameDates[i].presentSemaphore) != VK_SUCCESS)
        {
            std::cerr << "Vulkan: Failed to create present semaphore\n";
            return false;
        }

        if (vkCreateSemaphore(
            _device,
            &semaphoreCreateInfo,
            nullptr,
            &_frameDates[i].renderSemaphore) != VK_SUCCESS)
        {
            std::cerr << "Vulkan: Failed to create render semaphore\n";
            return false;
        }

        _deletionQueue.Push([=, this]()
        {
            vkDestroyFence(_device, _frameDates[i].renderFence, nullptr);
            vkDestroySemaphore(_device, _frameDates[i].presentSemaphore, nullptr);
            vkDestroySemaphore(_device, _frameDates[i].renderSemaphore, nullptr);
        });
    }

    if (vkCreateFence(
        _device,
        ToTempPtr(VkFenceCreateInfo
        {
            .sType = VkStructureType::VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
        }),
        nullptr,
        &_uploadContext.uploadFence) != VK_SUCCESS)
    {
        return false;
    }

    _deletionQueue.Push([=, this]()
    {
        vkDestroyFence(_device, _uploadContext.uploadFence, nullptr);
    });

    return true;
}

void Engine::Unload()
{
    vkDeviceWaitIdle(_device);

    _deletionQueue.Flush();
    
    for (size_t imageViewIndex = 0; imageViewIndex < _swapchainImageViews.size(); imageViewIndex++)
    {
        vkDestroyFramebuffer(_device, _framebuffers[imageViewIndex], nullptr);
        vkDestroyImageView(_device, _swapchainImageViews[imageViewIndex], nullptr);
    }

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

std::expected<VkShaderModule, std::string> Engine::LoadShaderModule(const std::string& filePath)
{
    auto buffer = ReadFile<uint32_t>(filePath);

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(
        _device,
        ToTempPtr(VkShaderModuleCreateInfo
        {
            .sType = VkStructureType::VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr,
            .codeSize = buffer.size(),
            .pCode = buffer.data()
        }),
        nullptr,
         &shaderModule) != VK_SUCCESS)
    {
        return std::unexpected("Vulkan: Failed to create shader module");
    }

    SetDebugName(_device, shaderModule, filePath);

    _deletionQueue.Push([=, this]()
    {
        vkDestroyShaderModule(_device, shaderModule, nullptr);
    });

    return shaderModule;
}

GLFWwindow* Engine::GetWindow()
{
    return _window;
}

void Engine::DrawRenderables(VkCommandBuffer commandBuffer, Renderable* first, size_t count)
{
    GpuPushConstants pushConstants;

    GpuCameraData gpuCameraData;
    gpuCameraData.projectionMatrix = glm::perspectiveFov(glm::pi<float>() / 2.0f, (float)_windowExtent.width, (float)_windowExtent.height, 0.1f, 512.0f);
    gpuCameraData.viewMatrix = glm::lookAtRH(glm::vec3(8, 7, 9), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    gpuCameraData.viewProjectionMatrix = gpuCameraData.projectionMatrix * gpuCameraData.viewMatrix;

    auto& currentFrame = GetCurrentFrameData();

    void* gpuCameraDataPtr = nullptr;
    if (vmaMapMemory(_allocator, currentFrame.cameraBuffer.allocation, &gpuCameraDataPtr) == VK_SUCCESS)
    {
        memcpy(gpuCameraDataPtr, &gpuCameraData, sizeof(GpuCameraData));
        vmaUnmapMemory(_allocator, currentFrame.cameraBuffer.allocation);
    }

    float arbitraryValue = (_frameIndex / 120.f);
    _gpuSceneData.ambientColor = { sin(arbitraryValue), 0, cos(arbitraryValue), 1 };

	char* gpuSceneDataPtr = {};
	if (vmaMapMemory(_allocator, _gpuSceneDataBuffer.allocation, (void**)&gpuSceneDataPtr) == VK_SUCCESS)
    {
	    uint32_t frameIndex = _frameIndex % FRAME_COUNT;
    	gpuSceneDataPtr += PadUniformBufferSize(sizeof(GpuSceneData)) * frameIndex;

        memcpy(gpuSceneDataPtr, &_gpuSceneData, sizeof(GpuSceneData));
        vmaUnmapMemory(_allocator, _gpuSceneDataBuffer.allocation);    
    }

    void* objectDataPtr = nullptr;
    if (vmaMapMemory(_allocator, currentFrame.objectBuffer.allocation, &objectDataPtr) == VK_SUCCESS)
    {
        GpuObjectData* gpuObjectDates = (GpuObjectData*)objectDataPtr;
        for (size_t i = 0; i < count; i++)
        {
            auto& renderable = first[i];
            //std::cout << "\nMesh: " << renderable.mesh->name << "\n" << glm::to_string(renderable.worldMatrix) << "\n";
            gpuObjectDates[i].worldMatrix = renderable.worldMatrix;
        }
        vmaUnmapMemory(_allocator, currentFrame.objectBuffer.allocation);
    }

    Mesh* lastMesh = nullptr;
    Pipeline* lastPipeline = nullptr;
    for (size_t i = 0; i < count; i++)
    {
        auto& renderable = first[i];

        if (lastPipeline == nullptr || lastPipeline->pipeline != renderable.pipeline.pipeline)
        {
            vkCmdBindPipeline(commandBuffer, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS, renderable.pipeline.pipeline);
            lastPipeline = &renderable.pipeline;

            uint32_t dynamicOffset = PadUniformBufferSize(sizeof(GpuSceneData)) * i;
            vkCmdBindDescriptorSets(commandBuffer, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS, lastPipeline->pipelineLayout, 0, 1, &currentFrame.globalDescriptorSet, 1, &dynamicOffset);
            vkCmdBindDescriptorSets(commandBuffer, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS, lastPipeline->pipelineLayout, 1, 1, &currentFrame.objectDescriptorSet, 0, nullptr);
        }

        if (renderable.mesh != lastMesh)
        {
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &renderable.mesh->vertexBuffer.buffer, &offset);
            vkCmdBindIndexBuffer(commandBuffer, renderable.mesh->indexBuffer.buffer, offset, VkIndexType::VK_INDEX_TYPE_UINT32);
            lastMesh = renderable.mesh;
        }

        pushConstants.worldMatrix = renderable.worldMatrix;
        vkCmdPushConstants(commandBuffer, renderable.pipeline.pipelineLayout, VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GpuPushConstants), &pushConstants);

        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(renderable.mesh->indices.size()), 1, 0, 0, 0);
    }

    vkCmdDrawIndexed(commandBuffer, lastMesh->indices.size(), count, 0, 0, 0);
}

std::vector<Mesh*> Engine::GetModel(const std::string& name)
{
    std::vector<Mesh*> meshes;

    auto it = _modelNameToMeshNameMap.find(name);
    if (it == _modelNameToMeshNameMap.end())
    {
        return meshes;
    }

    auto meshNames = (*it).second;
    for (auto& meshName : meshNames)
    {
        auto mesh = GetMesh(meshName);
        if (mesh != nullptr)
        {
            meshes.push_back(mesh);
        }
    }

    return meshes;
}

Mesh* Engine::GetMesh(const std::string& name)
{
    auto it = _meshNameToMeshMap.find(name);
    return it == _meshNameToMeshMap.end()
        ? nullptr
        : &(*it).second;
}

FrameData& Engine::GetCurrentFrameData()
{
    return _frameDates[_frameIndex % FRAMES_IN_FLIGHT];
}

size_t Engine::PadUniformBufferSize(size_t originalSize)
{
    size_t minUboAlignment = _physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
    size_t alignedSize = originalSize;
    if (minUboAlignment > 0)
    {
        alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
    }
    return alignedSize;
}

VkCommandBufferBeginInfo Engine::CreateCommandBufferBeginInfo(VkCommandBufferUsageFlags flags)
{
    VkCommandBufferBeginInfo commandBufferBeginInfo = {};
    commandBufferBeginInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.pNext = nullptr;
    commandBufferBeginInfo.pInheritanceInfo = nullptr;
    commandBufferBeginInfo.flags = flags;
    return commandBufferBeginInfo;
}

VkSubmitInfo Engine::CreateSubmitInfo(VkCommandBuffer* commandBuffer)
{
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;

    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = nullptr;
    submitInfo.pWaitDstStageMask = nullptr;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = commandBuffer;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = nullptr;

    return submitInfo;
}

void Engine::SubmitImmediately(std::function<void(VkCommandBuffer commandBuffer)>&& function)
{
    VkCommandBuffer commandBuffer = _uploadContext.commandBuffer;

    //begin the command buffer recording. We will use this command buffer exactly once before resetting, so we tell vulkan that
    VkCommandBufferBeginInfo commandBufferBeginInfo = CreateCommandBufferBeginInfo(VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    if (vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo) == VK_SUCCESS)
    {
        function(commandBuffer);

        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo = CreateSubmitInfo(&commandBuffer);

        // submit command buffer to the queue and execute it.
        // uploadFence will now block until the graphic commands finish execution
        if (vkQueueSubmit(_graphicsQueue, 1, &submitInfo, _uploadContext.uploadFence) == VK_SUCCESS)
        {
            vkWaitForFences(_device, 1, &_uploadContext.uploadFence, true, UINT64_MAX);
            vkResetFences(_device, 1, &_uploadContext.uploadFence);
            vkResetCommandPool(_device, _uploadContext.commandPool, 0);
        }
    }
}