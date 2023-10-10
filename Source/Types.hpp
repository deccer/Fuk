#pragma once

#include <vk_mem_alloc.h>
#include <volk.h>

#include <glm/mat4x4.hpp>

struct VertexInputDescription
{
	std::vector<VkVertexInputBindingDescription> Bindings;
	std::vector<VkVertexInputAttributeDescription> Attributes;

	VkPipelineVertexInputStateCreateFlags Flags = 0;
};

struct AllocatedBuffer
{
    VkBuffer Buffer;
    VmaAllocation Allocation;
};

struct AllocatedImage
{
	VkImage Image;
	VkImageView ImageView;
	VmaAllocation Allocation;
};

struct MeshPushConstants
{
	glm::mat4 ProjectionMatrix;
	glm::mat4 ViewMatrix;
	glm::mat4 WorldMatrix;
};