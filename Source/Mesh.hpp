#pragma once

#include <vector>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include "Types.hpp"

struct VertexPositionNormalColor
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec3 Color;

    static VertexInputDescription GetVertexInputDescription();
};

struct VertexPositionNormalUv
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 Uv;

    static VertexInputDescription GetVertexInputDescription();
};

struct Mesh
{
	std::vector<VertexPositionNormalUv> _vertices;
    std::vector<uint32_t> _indices;
	AllocatedBuffer _vertexBuffer;
    AllocatedBuffer _indexBuffer;

    glm::mat4 _worldMatrix;
};