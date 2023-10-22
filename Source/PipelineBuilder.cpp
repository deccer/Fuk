#include "PipelineBuilder.hpp"
#include "Engine.hpp"

#include <iostream>
#include <format>

VkPipelineShaderStageCreateInfo CreateShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule shaderModule)
{
    VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo = {};
    pipelineShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineShaderStageCreateInfo.pNext = nullptr;
    pipelineShaderStageCreateInfo.stage = stage;
    pipelineShaderStageCreateInfo.module = shaderModule;
    pipelineShaderStageCreateInfo.pName = "main";
    return pipelineShaderStageCreateInfo;
}

VkPipelineInputAssemblyStateCreateInfo CreateInputAssemblyCreateInfo(VkPrimitiveTopology topology)
{
    VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyState = {};
    pipelineInputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipelineInputAssemblyState.pNext = nullptr;

    pipelineInputAssemblyState.topology = topology;
    pipelineInputAssemblyState.primitiveRestartEnable = VK_FALSE;
    return pipelineInputAssemblyState;
}

VkPipelineRasterizationStateCreateInfo CreateRasterizationStateCreateInfo(VkPolygonMode polygonMode)
{
    VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo = {};
    pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    pipelineRasterizationStateCreateInfo.pNext = nullptr;

    pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    //discards all primitives before the rasterization stage if enabled which we don't want
    pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;

    pipelineRasterizationStateCreateInfo.polygonMode = polygonMode;
    pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;
    //no backface cull
    pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
    pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    //no depth bias
    pipelineRasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
    pipelineRasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
    pipelineRasterizationStateCreateInfo.depthBiasClamp = 0.0f;
    pipelineRasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;

    return pipelineRasterizationStateCreateInfo;
}

VkPipelineMultisampleStateCreateInfo CreateMultisampleStateCreateInfo()
{
    VkPipelineMultisampleStateCreateInfo pipelineMultisampleCreateInfo = {};
    pipelineMultisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    pipelineMultisampleCreateInfo.pNext = nullptr;

    pipelineMultisampleCreateInfo.sampleShadingEnable = VK_FALSE;
    //multisampling defaulted to no multisampling (1 sample per pixel)
    pipelineMultisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    pipelineMultisampleCreateInfo.minSampleShading = 1.0f;
    pipelineMultisampleCreateInfo.pSampleMask = nullptr;
    pipelineMultisampleCreateInfo.alphaToCoverageEnable = VK_FALSE;
    pipelineMultisampleCreateInfo.alphaToOneEnable = VK_FALSE;
    return pipelineMultisampleCreateInfo;
}

VkPipelineColorBlendAttachmentState CreateColorBlendAttachmentState()
{
    VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState = {};
    pipelineColorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                                       VK_COLOR_COMPONENT_G_BIT |
                                                       VK_COLOR_COMPONENT_B_BIT |
                                                       VK_COLOR_COMPONENT_A_BIT;
    pipelineColorBlendAttachmentState.blendEnable = VK_FALSE;
    return pipelineColorBlendAttachmentState;
}

PipelineBuilder& PipelineBuilder::WithGraphicsShadingStages(
        VkShaderModule vertexShaderModule,
        VkShaderModule fragmentShaderModule)
{
    _shaderStages.push_back(CreateShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShaderModule));
    _shaderStages.push_back(CreateShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShaderModule));    
    return *this;
}

PipelineBuilder& PipelineBuilder::WithVertexInput(const VertexInputDescription& vertexInputDescription)
{
    _vertexInputInfo = {};
    _vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    _vertexInputInfo.pNext = nullptr;

    _vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputDescription.bindings.size());
    _vertexInputInfo.pVertexBindingDescriptions = vertexInputDescription.bindings.data();
    _vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputDescription.attributes.size());
    _vertexInputInfo.pVertexAttributeDescriptions = vertexInputDescription.attributes.data();

    return *this;
}

PipelineBuilder& PipelineBuilder::WithTopology(VkPrimitiveTopology primitiveTopology)
{
    _inputAssembly = CreateInputAssemblyCreateInfo(primitiveTopology);
    return *this;
}

PipelineBuilder& PipelineBuilder::WithViewportAndScissor(VkViewport viewport, VkRect2D scissor)
{
    _viewport = viewport;
    _scissor = scissor;
    return *this;
}

PipelineBuilder& PipelineBuilder::WithPolygonMode(VkPolygonMode polygonMode)
{
    _rasterizer = CreateRasterizationStateCreateInfo(polygonMode);
    return *this;
}

PipelineBuilder& PipelineBuilder::WithoutMultisampling()
{
    _multisampling = CreateMultisampleStateCreateInfo();
    return *this;
}

PipelineBuilder& PipelineBuilder::WithoutBlending()
{
    _colorBlendAttachment = CreateColorBlendAttachmentState();
    return *this;
}

PipelineBuilder& PipelineBuilder::WithDepthTestingEnabled(VkCompareOp compareOperation)
{
    _depthStencil = {};
    _depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    _depthStencil.pNext = nullptr;

    _depthStencil.depthTestEnable = VK_TRUE;
    _depthStencil.depthWriteEnable = VK_TRUE;
    _depthStencil.depthCompareOp = compareOperation;
    _depthStencil.depthBoundsTestEnable = VK_FALSE;
    _depthStencil.minDepthBounds = 0.0f; // Optional
    _depthStencil.maxDepthBounds = 1.0f; // Optional
    _depthStencil.stencilTestEnable = VK_FALSE;
    return *this;
}

PipelineBuilder& PipelineBuilder::WithDescriptorSetLayout(VkDescriptorSetLayout descriptorSetLayout)
{
    _descriptorSetLayouts.push_back(descriptorSetLayout);
    return *this;
}

std::expected<Pipeline, std::string> PipelineBuilder::Build(
    const std::string& label,
    VkDevice device,
    VkRenderPass renderPass)
{
    if (_descriptorSetLayouts.empty())
    {
        return std::unexpected("Pipeline has no descriptor set layouts");
    }

    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(GpuPushConstants);
    pushConstantRange.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT;

    Pipeline pipeline;
    if (vkCreatePipelineLayout(
        device,
        ToTempPtr(VkPipelineLayoutCreateInfo
        {
            .sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .setLayoutCount = static_cast<uint32_t>(_descriptorSetLayouts.size()),
            .pSetLayouts = _descriptorSetLayouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushConstantRange,
        }),
        nullptr,
        &pipeline.pipelineLayout) != VK_SUCCESS)
    {
        return std::unexpected("Vulkan: Failed to create pipeline layout");
    }

    auto debugLabel = std::format("{}_PipelineLayout", label);
    SetDebugName(device, pipeline.pipelineLayout, debugLabel);    

    _deletionQueue.Push([=]()
    {
        vkDestroyPipelineLayout(device, pipeline.pipelineLayout, nullptr);
    });

    VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo = {};
    pipelineViewportStateCreateInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    pipelineViewportStateCreateInfo.pNext = nullptr;
    pipelineViewportStateCreateInfo.viewportCount = 1;
    pipelineViewportStateCreateInfo.pViewports = &_viewport;
    pipelineViewportStateCreateInfo.scissorCount = 1;
    pipelineViewportStateCreateInfo.pScissors = &_scissor;

    VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo = {};
    pipelineColorBlendStateCreateInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    pipelineColorBlendStateCreateInfo.pNext = nullptr;
    pipelineColorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
    pipelineColorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
    pipelineColorBlendStateCreateInfo.attachmentCount = 1;
    pipelineColorBlendStateCreateInfo.pAttachments = &_colorBlendAttachment;

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.pNext = nullptr;
    pipelineCreateInfo.stageCount = static_cast<uint32_t>(_shaderStages.size());
    pipelineCreateInfo.pStages = _shaderStages.data();
    pipelineCreateInfo.pVertexInputState = &_vertexInputInfo;
    pipelineCreateInfo.pInputAssemblyState = &_inputAssembly;
    pipelineCreateInfo.pViewportState = &pipelineViewportStateCreateInfo;
    pipelineCreateInfo.pRasterizationState = &_rasterizer;
    pipelineCreateInfo.pMultisampleState = &_multisampling;
    pipelineCreateInfo.pColorBlendState = &pipelineColorBlendStateCreateInfo;
    pipelineCreateInfo.pDepthStencilState = &_depthStencil;
    pipelineCreateInfo.layout = pipeline.pipelineLayout;
    pipelineCreateInfo.renderPass = renderPass;
    pipelineCreateInfo.subpass = 0;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(
        device,
        VK_NULL_HANDLE,
        1,
        &pipelineCreateInfo,
        nullptr,
        &pipeline.pipeline) != VK_SUCCESS)
    {
        pipeline.pipeline = VK_NULL_HANDLE;
        return std::unexpected("PipelineBuilder: Failed to create pipeline");
    }

    debugLabel = std::format("{}_Pipeline", label);
    SetDebugName(device, pipeline.pipeline, debugLabel);    

    _deletionQueue.Push([=]()
    {
        vkDestroyPipeline(device, pipeline.pipeline, nullptr);
    });

    return pipeline;   
}