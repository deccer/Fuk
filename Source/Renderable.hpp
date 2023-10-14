#pragma once

#include <glm/mat4x4.hpp>

struct Mesh;
struct Pipeline;

struct Renderable
{
    Mesh* mesh = nullptr;
    Pipeline* pipeline = nullptr;
    glm::mat4 worldMatrix = glm::mat4(1.0f);
};