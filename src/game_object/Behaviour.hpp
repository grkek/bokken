#pragma once

#include "Component.hpp"
#include "Base.hpp"
#include "Transform2D.hpp"

namespace Bokken
{
    namespace GameObject
    {
        // Abstract base for JS-authored game scripts.
        // Scripts query their own transform via gameObject->getComponent<Transform2D>().
        class Behaviour : public Component
        {
        public:
            void onAttach() override;

            virtual void onStart() {}
            virtual void onUpdate(float deltaTime) {}
            virtual void onFixedUpdate(float deltaTime) {}
        };
    }
}
