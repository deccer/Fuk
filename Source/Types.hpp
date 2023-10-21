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
    VkBuffer buffer = {};
    VmaAllocation allocation = {};
};

struct AllocatedImage
{
    VkImage image = {};
    VkImageView imageView = {};
    VmaAllocation allocation = {};
};

struct GpuPushConstants
{
    glm::mat4 worldMatrix;
};

struct GpuCameraData
{
    glm::mat4 projectionMatrix;
    glm::mat4 viewMatrix;
    glm::mat4 viewProjectionMatrix;
};

struct GpuSceneData
{
	glm::vec4 fogColorAndExponent;
	glm::vec4 fogDistances;
	glm::vec4 ambientColor;
	glm::vec4 sunlightDirectionAndPower;
	glm::vec4 sunlightColor;
};

template<class T>
T* ToTempPtr(T&& t)
{
    return &t;
}

template <typename>
struct VulkanObjectType;

template <>
struct VulkanObjectType<VkSwapchainKHR> { static constexpr const VkObjectType objectType = VK_OBJECT_TYPE_SWAPCHAIN_KHR; };

template <>
struct VulkanObjectType<VkQueue> { static constexpr const VkObjectType objectType = VK_OBJECT_TYPE_QUEUE; };

template <>
struct VulkanObjectType<VkCommandPool> { static constexpr const VkObjectType objectType = VK_OBJECT_TYPE_COMMAND_POOL; };

template <>
struct VulkanObjectType<VkCommandBuffer> { static constexpr const VkObjectType objectType = VK_OBJECT_TYPE_COMMAND_BUFFER; };

template <>
struct VulkanObjectType<VkRenderPass> { static constexpr const VkObjectType objectType = VK_OBJECT_TYPE_RENDER_PASS; };

template <>
struct VulkanObjectType<VkPipelineLayout> { static constexpr const VkObjectType objectType = VK_OBJECT_TYPE_PIPELINE_LAYOUT; };

template <>
struct VulkanObjectType<VkPipeline> { static constexpr const VkObjectType objectType = VK_OBJECT_TYPE_PIPELINE; };

template <>
struct VulkanObjectType<VkShaderModule> { static constexpr const VkObjectType objectType = VK_OBJECT_TYPE_SHADER_MODULE; };

template <>
struct VulkanObjectType<VkBuffer> { static constexpr const VkObjectType objectType = VK_OBJECT_TYPE_BUFFER; };

template <>
struct VulkanObjectType<VkBufferView> { static constexpr const VkObjectType objectType = VK_OBJECT_TYPE_BUFFER_VIEW; };

template <>
struct VulkanObjectType<VkImage> { static constexpr const VkObjectType objectType = VK_OBJECT_TYPE_IMAGE; };

template <>
struct VulkanObjectType<VkImageView> { static constexpr const VkObjectType objectType = VK_OBJECT_TYPE_IMAGE_VIEW; };

template <>
struct VulkanObjectType<VkDescriptorSetLayout> { static constexpr const VkObjectType objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT; };

template <>
struct VulkanObjectType<VkDescriptorSet> { static constexpr const VkObjectType objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET; };