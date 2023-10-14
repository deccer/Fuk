#pragma once

#include <volk.h>
#include <vector>
#include <expected>
#include <string>

#include "DeletionQueue.hpp"
#include "Types.hpp"
#include "Pipeline.hpp"

class PipelineBuilder
{
public:
    PipelineBuilder(const DeletionQueue& deletionQueue)
        : _deletionQueue(deletionQueue)
    {
    }

    PipelineBuilder& WithGraphicsShadingStages(VkShaderModule vertexShaderModule, VkShaderModule fragmentShaderModule);
    PipelineBuilder& WithVertexInput(const VertexInputDescription& vertexInputDescription);
    PipelineBuilder& WithTopology(VkPrimitiveTopology primitiveTopology);
    PipelineBuilder& WithViewportAndScissor(VkViewport viewport, VkRect2D scissor);
    PipelineBuilder& WithPolygonMode(VkPolygonMode polygonMode);    
    PipelineBuilder& WithoutMultisampling();
    PipelineBuilder& WithoutBlending();
    PipelineBuilder& WithDepthTestingEnabled(VkCompareOp compareOperation = VkCompareOp::VK_COMPARE_OP_LESS);
    PipelineBuilder& ForPipelineLayout(VkPipelineLayout pipelineLayout);

    std::expected<Pipeline, std::string> Build(VkDevice device, VkRenderPass renderPass);

private:
    DeletionQueue _deletionQueue;

    std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
    VkPipelineVertexInputStateCreateInfo _vertexInputInfo;
    VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
    VkViewport _viewport;
    VkRect2D _scissor;
    VkPipelineRasterizationStateCreateInfo _rasterizer;
    VkPipelineDepthStencilStateCreateInfo _depthStencil;
    VkPipelineColorBlendAttachmentState _colorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo _multisampling;
    VkPipelineLayout _pipelineLayout;
};