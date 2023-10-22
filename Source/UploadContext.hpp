#pragma once

#include <volk.h>

struct UploadContext
{
    VkFence uploadFence;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
};