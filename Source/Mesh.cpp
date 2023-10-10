#include "Mesh.hpp"

VertexInputDescription VertexPositionNormalColor::GetVertexInputDescription()
{
    VertexInputDescription vertexInputDescription = {};

    VkVertexInputBindingDescription inputBindingDescription = {};
    inputBindingDescription.binding = 0;
    inputBindingDescription.stride = sizeof(VertexPositionNormalColor);
    inputBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vertexInputDescription.Bindings.push_back(inputBindingDescription);

    VkVertexInputAttributeDescription positionAttribute = {};
    positionAttribute.binding = 0;
    positionAttribute.location = 0;
    positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    positionAttribute.offset = offsetof(VertexPositionNormalColor, Position);

    VkVertexInputAttributeDescription normalAttribute = {};
    normalAttribute.binding = 0;
    normalAttribute.location = 1;
    normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    normalAttribute.offset = offsetof(VertexPositionNormalColor, Normal);

    VkVertexInputAttributeDescription colorAttribute = {};
    colorAttribute.binding = 0;
    colorAttribute.location = 2;
    colorAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    colorAttribute.offset = offsetof(VertexPositionNormalColor, Color);

    vertexInputDescription.Attributes.push_back(positionAttribute);
    vertexInputDescription.Attributes.push_back(normalAttribute);
    vertexInputDescription.Attributes.push_back(colorAttribute);

    return vertexInputDescription;
}

VertexInputDescription VertexPositionNormalUv::GetVertexInputDescription()
{
    VertexInputDescription vertexInputDescription = {};

    VkVertexInputBindingDescription inputBindingDescription = {};
    inputBindingDescription.binding = 0;
    inputBindingDescription.stride = sizeof(VertexPositionNormalUv);
    inputBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vertexInputDescription.Bindings.push_back(inputBindingDescription);

    VkVertexInputAttributeDescription positionAttribute = {};
    positionAttribute.binding = 0;
    positionAttribute.location = 0;
    positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    positionAttribute.offset = offsetof(VertexPositionNormalUv, Position);

    VkVertexInputAttributeDescription normalAttribute = {};
    normalAttribute.binding = 0;
    normalAttribute.location = 1;
    normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    normalAttribute.offset = offsetof(VertexPositionNormalUv, Normal);

    VkVertexInputAttributeDescription uvAttribute = {};
    uvAttribute.binding = 0;
    uvAttribute.location = 2;
    uvAttribute.format = VK_FORMAT_R32G32_SFLOAT;
    uvAttribute.offset = offsetof(VertexPositionNormalUv, Uv);

    vertexInputDescription.Attributes.push_back(positionAttribute);
    vertexInputDescription.Attributes.push_back(normalAttribute);
    vertexInputDescription.Attributes.push_back(uvAttribute);

    return vertexInputDescription;
}