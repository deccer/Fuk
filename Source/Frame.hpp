#pragma once

#include <volk.h>

struct Frame
{
    VkSemaphore presentSemaphore = {};
    VkSemaphore renderSemaphore = {};
    VkFence renderFence = {};	

    VkCommandPool commandPool = {};
    VkCommandBuffer commandBuffer = {};
};