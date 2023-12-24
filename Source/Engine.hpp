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

private:
    bool InitializeVulkan();

    DeletionQueue _deletionQueue;

    VkExtent2D _windowExtent{1920, 1080};
    bool _vsync{true};
    std::string _windowTitle{"Fuk"};

    GLFWwindow* _window{nullptr};

    VmaAllocator _allocator;
    VkInstance _instance;
#ifdef _DEBUG
    VkDebugUtilsMessengerEXT _debugMessenger;
#endif
    VkPhysicalDevice _physicalDevice;
    VkPhysicalDeviceProperties _physicalDeviceProperties;
    VkDevice _device;
    VkSurfaceKHR _surface;

    VkQueue _graphicsQueue;
    uint32_t _graphicsQueueFamily;
};