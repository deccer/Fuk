#pragma once

#include <vk_mem_alloc.h>
#include <volk.h>

#include <glm/mat4x4.hpp>

struct VertexInputDescription
{
	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attributes;

	VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct AllocatedBuffer
{
    VkBuffer buffer;
    VmaAllocation allocation;
};

struct AllocatedImage
{
	VkImage image;
	VkImageView imageView;
	VmaAllocation allocation;
};

struct MeshPushConstants
{
	glm::mat4 projectionMatrix;
	glm::mat4 viewMatrix;
	glm::mat4 worldMatrix;
};