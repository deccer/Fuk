#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vk_mem_alloc.h>
#include <volk.h>
#include <VkBootstrap.h>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <vector>
#include <cstdint>
#include <string>
#include <expected>
#include <unordered_map>

#include "DeletionQueue.hpp"
#include "Types.hpp"
#include "Pipeline.hpp"
#include "Mesh.hpp"
#include "Renderable.hpp"
#include "Frame.hpp"

constexpr uint32_t FRAME_COUNT = 2;

class Engine
{
public:
    bool Initialize();
    bool Load();
    bool Draw();
    void Unload();

    GLFWwindow* GetWindow();

    Mesh* GetMesh(const std::string& name);

private:
    std::vector<Renderable> _renderables;
    std::unordered_map<std::string, std::vector<std::string>> _modelNameToMeshNameMap;
    std::unordered_map<std::string, Mesh> _meshNameToMeshMap;

    int32_t _frameIndex{0};
    VkExtent2D _windowExtent{1920, 1080};
    bool _vsync{true};
    std::string _windowTitle{"Fuk"};
    DeletionQueue _deletionQueue;

    GLFWwindow* _window{nullptr};
    VkInstance _instance;
#ifdef _DEBUG
    VkDebugUtilsMessengerEXT _debugMessenger;
#endif
    VkPhysicalDevice _physicalDevice;
    VkDevice _device;
    VkSurfaceKHR _surface;

    VkSwapchainKHR _swapchain;
    VkFormat _swapchainImageFormat;
    std::vector<VkImage> _swapchainImages;
    std::vector<VkImageView> _swapchainImageViews;

    AllocatedImage _depthImage;
    VkFormat _depthFormat;

    VkQueue _graphicsQueue;
    uint32_t _graphicsQueueFamily;

    VkRenderPass _renderPass;
    std::vector<VkFramebuffer> _framebuffers;

    VkShaderModule _simpleVertexShaderModule;
    VkShaderModule _simpleFragmentShaderModule;

    VkPipelineLayout _trianglePipelineLayout;
    VkPipeline _trianglePipeline;

    VmaAllocator _allocator;

    VkPipeline _meshPipeline;

    Frame _frames[FRAME_COUNT];
    Frame& GetCurrentFrame();

    template<typename T>
    std::expected<AllocatedBuffer, std::string> CreateBuffer(
        const std::string& label,
        VkBufferUsageFlagBits bufferUsageFlagBits,
        VmaMemoryUsage memoryUsage,
        std::vector<T> data)
    {
        VkBufferCreateInfo bufferCreateInfo = {};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = data.size() * sizeof(T);
        bufferCreateInfo.usage = bufferUsageFlagBits;

        VmaAllocationCreateInfo bufferAllocationCreateInfo = {};
        bufferAllocationCreateInfo.usage = memoryUsage;

        AllocatedBuffer buffer;
        if (vmaCreateBuffer(_allocator, &bufferCreateInfo, &bufferAllocationCreateInfo,
            &buffer.buffer,
            &buffer.allocation,
            nullptr) != VK_SUCCESS)
        {
            return std::unexpected("Vulkan: Failed to create buffer");
        }

        SetDebugName(VkObjectType::VK_OBJECT_TYPE_BUFFER, buffer.buffer, label);

        void* dataPtr = nullptr;
        if (vmaMapMemory(_allocator, buffer.allocation, &dataPtr) != VK_SUCCESS)
        {
            return std::unexpected("Vulkan: Failed to map buffer");
        }

        memcpy(dataPtr, data.data(), data.size() * sizeof(T));
        vmaUnmapMemory(_allocator, buffer.allocation);    

        _deletionQueue.Push([=]()
        {
            vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
        });

        return buffer;
    }

    std::expected<AllocatedImage, std::string> CreateImage(
        const std::string& label,
        VkFormat format,
        VkImageUsageFlagBits imageUsageFlags,
        VkImageAspectFlagBits imageAspectFlags,
        VkExtent3D extent);

    bool InitializeVulkan();
    bool InitializeSwapchain();
    bool InitializeCommandBuffers();
    bool InitializeRenderPass();
    bool InitializeFramebuffers();
    bool InitializeSynchronizationStructures();
    bool InitializePipelines();

    bool LoadMeshFromFile(const std::string& modelName, const std::string& filePath);

    bool TryLoadShaderModule(const std::string& filePath, VkShaderModule* shaderModule);
    void SetDebugName(VkObjectType objectType, void* object, const std::string& debugName);

    void DrawRenderables(VkCommandBuffer commandBuffer, Renderable* first, size_t count);
};