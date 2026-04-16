#pragma once

namespace Bokken
{
    namespace GameObject
    {
        class Base;
    }

    namespace GameObject
    {
        class Component
        {
        public:
            virtual ~Component() = default;

            // Back-pointer to the owning GameObject (set by GameObject::addComponent)
            Base *gameObject = nullptr;

            // Mirrors the TypeScript `enabled` flag.
            // C++ systems and JS lifecycle hooks must check this before acting.
            bool enabled = true;

            // Called once by the engine immediately after the component is attached
            // and its gameObject pointer is set. Override in subclasses.
            virtual void onAttach() {}

            // Called by the engine each frame. deltaTime in seconds.
            virtual void update(float deltaTime) {}

            // Called at a fixed timestep for physics work.
            virtual void fixedUpdate(float deltaTime) {}

            // Called immediately before the component/owning object is destroyed.
            virtual void onDestroy() {}
        };
    }
}