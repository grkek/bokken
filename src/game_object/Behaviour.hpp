#pragma once

#include "Component.hpp"
#include "Base.hpp"
#include "Transform.hpp"

namespace Bokken
{
    namespace GameObject
    {
        // Abstract base for all JS-authored game scripts.
        // The C++ engine calls the virtual hooks each frame; the JS binding
        // module overrides them to dispatch into the QuickJS VM.
        class Behaviour : public Component
        {
        public:
            // Shortcut to the owning object's Transform — set in onAttach().
            Transform *transform = nullptr;

            void onAttach() override;

            // These mirror the optional TS lifecycle hooks.
            // A concrete JS-bound subclass overrides whichever it needs.
            virtual void onStart() {}
            virtual void onUpdate(float deltaTime) {}
            virtual void onFixedUpdate(float deltaTime) {}
            // onDestroy() is inherited from Component
        };
    }
}