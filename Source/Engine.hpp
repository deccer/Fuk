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
#include "FrameData.hpp"

constexpr uint32_t FRAME_COUNT = 2;

template<typename T>
void SetDebugName(VkDevice device, T object, const std::string& debugName)
{
#ifdef _DEBUG
    const VkDebugUtilsObjectNameInfoEXT debugUtilsObjectNameInfo =
    {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .pNext = NULL,
        .objectType = VulkanObjectType<T>::objectType,
        .objectHandle = (uint64_t)object,
        .pObjectName = debugName.c_str(),
    };

    vkSetDebugUtilsObjectNameEXT(device, &debugUtilsObjectNameInfo);
#endif
}

class Engine
{
public:
    bool Initialize();
    bool Load();
    bool Draw();
    void Unload();

    GLFWwindow* GetWindow();

    Mesh* GetMesh(const std::string& name);
    std::vector<Mesh*> GetModel(const std::string& name);

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
    VkPhysicalDeviceProperties _physicalDeviceProperties;
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

    VkDescriptorSetLayout _globalDescriptorSetLayout;
    VkDescriptorSetLayout _objectDescriptorSetLayout;
    VkDescriptorPool _descriptorPool;    

    VkShaderModule _simpleVertexShaderModule;
    VkShaderModule _simpleFragmentShaderModule;

    VmaAllocator _allocator;

    VkPipeline _meshPipeline;

    GpuSceneData _gpuSceneData;
    AllocatedBuffer _gpuSceneDataBuffer;

    FrameData _frameDates[FRAME_COUNT];
    FrameData& GetCurrentFrameData();

    size_t PadUniformBufferSize(size_t originalSize);

    template<typename TData>
    std::expected<AllocatedBuffer, std::string> CreateBuffer(
        const std::string& label,
        VmaMemoryUsage memoryUsage,
        std::span<TData> data)
    {
        AllocatedBuffer buffer;
        if (vmaCreateBuffer(
            _allocator,
            ToTempPtr(VkBufferCreateInfo
            {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = data.size() * sizeof(TData),
                .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
            }),
            ToTempPtr(VmaAllocationCreateInfo
            {
                .usage = memoryUsage
            }),
            &buffer.buffer,
            &buffer.allocation,
            nullptr) != VK_SUCCESS)
        {
            return std::unexpected("Vulkan: Failed to create buffer");
        }

        SetDebugName(_device, buffer.buffer, label);

        void* dataPtr = nullptr;
        if (vmaMapMemory(_allocator, buffer.allocation, &dataPtr) != VK_SUCCESS)
        {
            return std::unexpected("Vulkan: Failed to map buffer");
        }

        memcpy(dataPtr, data.data(), data.size() * sizeof(TData));
        vmaUnmapMemory(_allocator, buffer.allocation);    

        _deletionQueue.Push([=]()
        {
            vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
        });

        return buffer;
    }

    template<typename TData>
    std::expected<AllocatedBuffer, std::string> CreateBuffer(
        const std::string& label,
        std::size_t dataSize,
        VmaMemoryUsage memoryUsage)
    {
        AllocatedBuffer buffer;
        if (vmaCreateBuffer(
            _allocator,
            ToTempPtr(VkBufferCreateInfo
            {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = dataSize,
                .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
            }),
            ToTempPtr(VmaAllocationCreateInfo
            {
                .usage = memoryUsage
            }),
            &buffer.buffer,
            &buffer.allocation,
            nullptr) != VK_SUCCESS)
        {
            return std::unexpected("Vulkan: Failed to create buffer");
        }

        SetDebugName(_device, buffer.buffer, label);

        _deletionQueue.Push([=]()
        {
            vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
        });

        return buffer;
    }

    template<typename TData>
    std::expected<AllocatedBuffer, std::string> CreateBuffer(
        const std::string& label,
        VmaMemoryUsage memoryUsage)
    {
        AllocatedBuffer buffer;
        if (vmaCreateBuffer(
            _allocator,
            ToTempPtr(VkBufferCreateInfo
            {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = sizeof(TData),
                .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
            }),
            ToTempPtr(VmaAllocationCreateInfo
            {
                .usage = memoryUsage
            }),
            &buffer.buffer,
            &buffer.allocation,
            nullptr) != VK_SUCCESS)
        {
            return std::unexpected("Vulkan: Failed to create buffer");
        }

        SetDebugName(_device, buffer.buffer, label);

        _deletionQueue.Push([=]()
        {
            vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
        });

        return buffer;
    }

    std::expected<AllocatedImage, std::string> CreateImage(
        const std::string& label,
        VkFormat format,
        VkImageUsageFlags imageUsageFlags,
        VkImageAspectFlags imageAspectFlags,
        VkExtent3D extent);

    bool InitializeVulkan();
    bool InitializeSwapchain();
    bool InitializeCommandBuffers();
    bool InitializeRenderPass();
    bool InitializeFramebuffers();
    bool InitializeDescriptors();
    bool InitializeSynchronizationStructures();

    bool LoadMeshFromFile(const std::string& modelName, const std::string& filePath);

    std::expected<VkShaderModule, std::string> LoadShaderModule(const std::string& filePath);

    void DrawRenderables(VkCommandBuffer commandBuffer, Renderable* first, size_t count);
};