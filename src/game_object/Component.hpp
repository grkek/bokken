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

            // Back-pointer set by GameObject::addComponent.
            Base *gameObject = nullptr;

            bool enabled = true;

            virtual void onAttach() {}
            virtual void update(float deltaTime) {}
            virtual void fixedUpdate(float deltaTime) {}
            virtual void onDestroy() {}
        };
    }
}
