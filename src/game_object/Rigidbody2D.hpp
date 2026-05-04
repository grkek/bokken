#pragma once

#include "Component.hpp"
#include "Transform2D.hpp"
#include "Base.hpp"
#include <glm/glm.hpp>

namespace Bokken
{
    namespace GameObject
    {
        // 2D rigid-body dynamics in the XY plane.
        // Rigidbody3D will extend to full 3D vectors and quaternion angular velocity.
        class Rigidbody2D : public Component
        {
        public:
            float mass       = 1.0f;
            bool  useGravity = true;
            bool  isStatic   = false;

            glm::vec2 velocity{0.0f};
            float     angularVelocity = 0.0f; // Degrees/sec

            void applyForce(float x, float y)
            {
                if (isStatic) return;
                m_pendingForce += glm::vec2(x, y);
            }

            void applyTorque(float degrees)
            {
                if (isStatic) return;
                m_pendingTorque += degrees;
            }

            void setVelocity(float x, float y)
            {
                velocity = glm::vec2(x, y);
            }

            void fixedUpdate(float dt) override
            {
                if (!enabled || isStatic) return;

                if (useGravity)
                    m_pendingForce += glm::vec2(0.0f, -9.81f * mass);

                velocity        += (m_pendingForce / mass) * dt;
                angularVelocity += (m_pendingTorque / mass) * dt;

                m_pendingForce  = glm::vec2(0.0f);
                m_pendingTorque = 0.0f;

                if (gameObject)
                {
                    auto *t = gameObject->getComponent<Transform2D>();
                    if (t)
                    {
                        t->position += velocity * dt;
                        t->rotation += angularVelocity * dt;
                    }
                }
            }

        private:
            glm::vec2 m_pendingForce{0.0f};
            float     m_pendingTorque = 0.0f;
        };
    }
}
