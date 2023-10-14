#pragma once

#include <glm/mat4x4.hpp>

struct Mesh;
struct Pipeline;

struct Renderable
{
    Mesh* mesh;
    Pipeline* pipeline;
    glm::mat4 worldMatrix;
};