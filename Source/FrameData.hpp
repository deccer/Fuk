#pragma once

#include <volk.h>

#include "Types.hpp"

struct FrameData
{
    VkSemaphore presentSemaphore = {};
    VkSemaphore renderSemaphore = {};
    VkFence renderFence = {};	

    VkCommandPool commandPool = {};
    VkCommandBuffer commandBuffer = {};

    AllocatedBuffer cameraBuffer = {};
    VkDescriptorSet globalDescriptorSet;
};