#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <volk.h>
#include <VkBootstrap.h>

#include <vector>
#include <cstdint>
#include <string>

#include "DeletionQueue.hpp"

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

    bool InitializeVulkan();
    bool InitializeSwapchain();
    bool InitializeCommandBuffers();
    bool InitializeRenderPass();
    bool InitializeFramebuffers();
    bool InitializeSynchronizationStructures();
    bool InitializePipelines();

    bool TryLoadShaderModule(const std::string& filePath, VkShaderModule* shaderModule);
    void SetDebugName(VkObjectType objectType, void* object, const std::string& debugName);
};