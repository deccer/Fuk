#pragma once

#include <volk.h>
#include <vector>

#include "DeletionQueue.hpp"
#include "Types.hpp"

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

    bool TryBuild(VkDevice device, VkRenderPass renderPass, VkPipeline* pipeline);

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