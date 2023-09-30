#pragma once

#if defined(_WIN32)
#include <fcntl.h>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif // _WIN32

#if defined(__linux__) || defined(__APPLE__)
#include <dlfcn.h>
#endif

#include <VkBootstrap.h>

struct VulkanBindings
{
#if defined(__linux__) || defined(__APPLE__)
    void* LibraryHandle;
#elif defined(_WIN32)
    HMODULE LibraryHandle;
#endif

    VulkanBindings()
    {
    #if defined(__linux__)
        LibraryHandle = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
        if (!LibraryHandle)
        {
            LibraryHandle = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
        }
    #elif defined(__APPLE__)
        LibraryHandle = dlopen("libvulkan.dylib", RTLD_NOW | RTLD_LOCAL);
        if (!LibraryHandle)
        {
            LibraryHandle = dlopen("libvulkan.1.dylib", RTLD_NOW | RTLD_LOCAL);
        }
    #elif defined(_WIN32)
        LibraryHandle = LoadLibrary(TEXT("vulkan-1.dll"));
    #else
        assert(false && "Unsupported platform");
    #endif
        if (!LibraryHandle)
        {
            return;
        }
    #if defined(__linux__) || defined(__APPLE__)
        vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(LibraryHandle, "vkGetInstanceProcAddr"));
    #elif defined(_WIN32)
        vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(LibraryHandle, "vkGetInstanceProcAddr"));
    #endif
    }

    void Close()
    {
    #if defined(__linux__) || defined(__APPLE__)
        dlclose(LibraryHandle);
    #elif defined(_WIN32)
        FreeLibrary(LibraryHandle);
    #endif
        LibraryHandle = nullptr;
    }

    void InitalizeFromInstance(VkInstance instance)
    {
        vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)vkGetInstanceProcAddr(instance, "vkGetDeviceProcAddr");
        vkDestroySurfaceKHR = (PFN_vkDestroySurfaceKHR)vkGetInstanceProcAddr(instance, "vkDestroySurfaceKHR");
    }

    void InitializeFromDevice(VkDevice device)
    {
        vkCreateRenderPass = (PFN_vkCreateRenderPass)vkGetDeviceProcAddr(device, "vkCreateRenderPass");
        vkCreateShaderModule = (PFN_vkCreateShaderModule)vkGetDeviceProcAddr(device, "vkCreateShaderModule");
        vkCreatePipelineLayout = (PFN_vkCreatePipelineLayout)vkGetDeviceProcAddr(device, "vkCreatePipelineLayout");
        vkCreateGraphicsPipelines = (PFN_vkCreateGraphicsPipelines)vkGetDeviceProcAddr(device, "vkCreateGraphicsPipelines");
        vkDestroyShaderModule = (PFN_vkDestroyShaderModule)vkGetDeviceProcAddr(device, "vkDestroyShaderModule");
        vkCreateFramebuffer = (PFN_vkCreateFramebuffer)vkGetDeviceProcAddr(device, "vkCreateFramebuffer");
        vkCreateCommandPool = (PFN_vkCreateCommandPool)vkGetDeviceProcAddr(device, "vkCreateCommandPool");
        vkAllocateCommandBuffers = (PFN_vkAllocateCommandBuffers)vkGetDeviceProcAddr(device, "vkAllocateCommandBuffers");
        vkBeginCommandBuffer = (PFN_vkBeginCommandBuffer)vkGetDeviceProcAddr(device, "vkBeginCommandBuffer");
        vkEndCommandBuffer = (PFN_vkEndCommandBuffer)vkGetDeviceProcAddr(device, "vkEndCommandBuffer");
        vkCmdSetViewport = (PFN_vkCmdSetViewport)vkGetDeviceProcAddr(device, "vkCmdSetViewport");
        vkCmdSetScissor = (PFN_vkCmdSetScissor)vkGetDeviceProcAddr(device, "vkCmdSetScissor");
        vkCmdBeginRenderPass = (PFN_vkCmdBeginRenderPass)vkGetDeviceProcAddr(device, "vkCmdBeginRenderPass");
        vkCmdEndRenderPass = (PFN_vkCmdEndRenderPass)vkGetDeviceProcAddr(device, "vkCmdEndRenderPass");
        vkCmdBindPipeline = (PFN_vkCmdBindPipeline)vkGetDeviceProcAddr(device, "vkCmdBindPipeline");
        vkCmdDraw = (PFN_vkCmdDraw)vkGetDeviceProcAddr(device, "vkCmdDraw");
        vkCreateSemaphore = (PFN_vkCreateSemaphore)vkGetDeviceProcAddr(device, "vkCreateSemaphore");
        vkCreateFence = (PFN_vkCreateFence)vkGetDeviceProcAddr(device, "vkCreateFence");
        vkDeviceWaitIdle = (PFN_vkDeviceWaitIdle)vkGetDeviceProcAddr(device, "vkDeviceWaitIdle");
        vkDestroyCommandPool = (PFN_vkDestroyCommandPool)vkGetDeviceProcAddr(device, "vkDestroyCommandPool");
        vkDestroyFramebuffer = (PFN_vkDestroyFramebuffer)vkGetDeviceProcAddr(device, "vkDestroyFramebuffer");
        vkWaitForFences = (PFN_vkWaitForFences)vkGetDeviceProcAddr(device, "vkWaitForFences");
        vkAcquireNextImageKHR = (PFN_vkAcquireNextImageKHR)vkGetDeviceProcAddr(device, "vkAcquireNextImageKHR");
        vkResetFences = (PFN_vkResetFences)vkGetDeviceProcAddr(device, "vkResetFences");
        vkQueueSubmit = (PFN_vkQueueSubmit)vkGetDeviceProcAddr(device, "vkQueueSubmit");
        vkQueuePresentKHR = (PFN_vkQueuePresentKHR)vkGetDeviceProcAddr(device, "vkQueuePresentKHR");
        vkDestroySemaphore = (PFN_vkDestroySemaphore)vkGetDeviceProcAddr(device, "vkDestroySemaphore");
        vkDestroyFence = (PFN_vkDestroyFence)vkGetDeviceProcAddr(device, "vkDestroyFence");
        vkDestroyPipeline = (PFN_vkDestroyPipeline)vkGetDeviceProcAddr(device, "vkDestroyPipeline");
        vkDestroyPipelineLayout = (PFN_vkDestroyPipelineLayout)vkGetDeviceProcAddr(device, "vkDestroyPipelineLayout");
        vkDestroyRenderPass = (PFN_vkDestroyRenderPass)vkGetDeviceProcAddr(device, "vkDestroyRenderPass");
    }

    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = VK_NULL_HANDLE;
    PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr = VK_NULL_HANDLE;

    PFN_vkCreateRenderPass vkCreateRenderPass = VK_NULL_HANDLE;
    PFN_vkCreateShaderModule vkCreateShaderModule = VK_NULL_HANDLE;
    PFN_vkCreatePipelineLayout vkCreatePipelineLayout = VK_NULL_HANDLE;
    PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines = VK_NULL_HANDLE;
    PFN_vkDestroyShaderModule vkDestroyShaderModule = VK_NULL_HANDLE;
    PFN_vkCreateFramebuffer vkCreateFramebuffer = VK_NULL_HANDLE;
    PFN_vkCreateCommandPool vkCreateCommandPool = VK_NULL_HANDLE;
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers = VK_NULL_HANDLE;
    PFN_vkBeginCommandBuffer vkBeginCommandBuffer = VK_NULL_HANDLE;
    PFN_vkEndCommandBuffer vkEndCommandBuffer = VK_NULL_HANDLE;
    PFN_vkCmdSetViewport vkCmdSetViewport = VK_NULL_HANDLE;
    PFN_vkCmdSetScissor vkCmdSetScissor = VK_NULL_HANDLE;
    PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass = VK_NULL_HANDLE;
    PFN_vkCmdEndRenderPass vkCmdEndRenderPass = VK_NULL_HANDLE;
    PFN_vkCmdBindPipeline vkCmdBindPipeline = VK_NULL_HANDLE;
    PFN_vkCmdDraw vkCmdDraw = VK_NULL_HANDLE;
    PFN_vkCreateSemaphore vkCreateSemaphore = VK_NULL_HANDLE;
    PFN_vkCreateFence vkCreateFence = VK_NULL_HANDLE;
    PFN_vkDeviceWaitIdle vkDeviceWaitIdle = VK_NULL_HANDLE;
    PFN_vkDestroyCommandPool vkDestroyCommandPool = VK_NULL_HANDLE;
    PFN_vkDestroyFramebuffer vkDestroyFramebuffer = VK_NULL_HANDLE;
    PFN_vkWaitForFences vkWaitForFences = VK_NULL_HANDLE;
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR = VK_NULL_HANDLE;
    PFN_vkResetFences vkResetFences = VK_NULL_HANDLE;
    PFN_vkQueueSubmit vkQueueSubmit = VK_NULL_HANDLE;
    PFN_vkQueuePresentKHR vkQueuePresentKHR = VK_NULL_HANDLE;
    PFN_vkDestroySemaphore vkDestroySemaphore = VK_NULL_HANDLE;
    PFN_vkDestroyFence vkDestroyFence = VK_NULL_HANDLE;
    PFN_vkDestroyPipeline vkDestroyPipeline = VK_NULL_HANDLE;
    PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout = VK_NULL_HANDLE;
    PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR = VK_NULL_HANDLE;
    PFN_vkDestroyRenderPass vkDestroyRenderPass = VK_NULL_HANDLE;
};