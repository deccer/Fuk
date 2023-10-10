#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <volk.h>
#include <VkBootstrap.h>

#include <vector>
#include <cstdint>
#include <string>
#include <expected>

#include <vk_mem_alloc.h>

#include "DeletionQueue.hpp"
#include "Mesh.hpp"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

class Engine
{
public:
    bool Initialize();
    bool Load();
    bool Draw();
    void Unload();

    GLFWwindow* GetWindow();    

private:
    int32_t _frameIndex{0};
    VkExtent2D _windowExtent{1920, 1080};
    bool _vsync{true};
    std::string _windowTitle{"Fuk"};
    DeletionQueue _deletionQueue;

    GLFWwindow* _window;
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

    VkCommandPool _commandPool;
    VkCommandBuffer _mainCommandBuffer;

    VkRenderPass _renderPass;
    std::vector<VkFramebuffer> _framebuffers;

    VkSemaphore _presentSemaphore;
    VkSemaphore _renderSemaphore;
    VkFence _renderFence;

    VkShaderModule _simpleVertexShaderModule;
    VkShaderModule _simpleFragmentShaderModule;

    VkPipelineLayout _trianglePipelineLayout;
    VkPipeline _trianglePipeline;

    VmaAllocator _allocator;

    VkPipeline _meshPipeline;
    std::vector<Mesh> _meshes;

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
            &buffer.Buffer,
            &buffer.Allocation,
            nullptr) != VK_SUCCESS)
        {
            return std::unexpected("Vulkan: Failed to create buffer");
        }

        SetDebugName(VkObjectType::VK_OBJECT_TYPE_BUFFER, buffer.Buffer, label);

        void* dataPtr = nullptr;
        if (vmaMapMemory(_allocator, buffer.Allocation, &dataPtr) != VK_SUCCESS)
        {
            return std::unexpected("Vulkan: Failed to map buffer");
        }

        memcpy(dataPtr, data.data(), data.size() * sizeof(T));
        vmaUnmapMemory(_allocator, buffer.Allocation);    

        _deletionQueue.Push([=]()
        {
            vmaDestroyBuffer(_allocator, buffer.Buffer, buffer.Allocation);
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

    bool LoadMeshFromFile(const std::string& filePath);

    bool TryLoadShaderModule(const std::string& filePath, VkShaderModule* shaderModule);
    void SetDebugName(VkObjectType objectType, void* object, const std::string& debugName);
};