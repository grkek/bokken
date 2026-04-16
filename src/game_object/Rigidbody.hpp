#pragma once
#include "Component.hpp"
#include <glm/glm.hpp>

namespace Bokken
{
    namespace GameObject
    {
        class Rigidbody : public Component
        {
        public:
            float mass = 1.0f;
            bool useGravity = true;
            bool isStatic = false;

            glm::vec3 velocity{0.0f};

            void applyForce(float x, float y, float z)
            {
                if (isStatic)
                    return;
                m_pendingForce += glm::vec3(x, y, z);
            }

            void setVelocity(float x, float y, float z)
            {
                velocity = glm::vec3(x, y, z);
            }

            void fixedUpdate(float dt) override
            {
                if (!enabled || isStatic)
                    return;

                // Gravity
                if (useGravity)
                    m_pendingForce += glm::vec3(0, -9.81f * mass, 0);

                // Integrate
                velocity += (m_pendingForce / mass) * dt;
                m_pendingForce = glm::vec3(0.0f);

                // Move the Transform (if present)
                if (gameObject)
                {
                    auto *t = gameObject->getComponent<Transform>();
                    if (t)
                        t->position += velocity * dt;
                }
            }

        private:
            glm::vec3 m_pendingForce{0.0f};
        };
    }
}