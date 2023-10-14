#pragma once

#include <vector>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include "Types.hpp"

struct VertexPositionNormalColor
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;

    static VertexInputDescription GetVertexInputDescription();
};

struct VertexPositionNormalUv
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;

    static VertexInputDescription GetVertexInputDescription();
};

struct Mesh
{
	std::vector<VertexPositionNormalUv> vertices;
    std::vector<uint32_t> indices;
	AllocatedBuffer vertexBuffer;
    AllocatedBuffer indexBuffer;

    glm::mat4 worldMatrix;
};