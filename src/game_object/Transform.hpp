#pragma once
#include "Component.hpp"
#include "Mesh.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Bokken
{
    namespace GameObject
    {
        class Transform : public Component
        {
        public:
            glm::vec3 position{0.0f};
            glm::vec3 rotation{0.0f}; // Euler degrees, matches TS API
            glm::vec3 scale{1.0f};

            // The mesh to render — kept here so the renderer can find it
            // alongside the transform in one lookup.
            Mesh mesh = Mesh::Empty;

            // Mirrors TS translate(x,y,z) — single C++ vector addition
            void translate(float x, float y, float z)
            {
                position += glm::vec3(x, y, z);
            }

            void rotate(float x, float y, float z)
            {
                rotation += glm::vec3(x, y, z);
            }

            glm::mat4 getMatrix() const
            {
                glm::mat4 m = glm::mat4(1.0f);
                m = glm::translate(m, position);
                m = glm::rotate(m, glm::radians(rotation.x), {1, 0, 0});
                m = glm::rotate(m, glm::radians(rotation.y), {0, 1, 0});
                m = glm::rotate(m, glm::radians(rotation.z), {0, 0, 1});
                m = glm::scale(m, scale);
                return m;
            }
        };
    }
}