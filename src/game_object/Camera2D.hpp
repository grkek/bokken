#pragma once

#include "Component.hpp"

namespace Bokken
{
    namespace GameObject
    {
        // Camera for 2D rendering. Attach alongside a Transform2D to control
        // the viewport. The first Camera2D found with isActive == true is used
        // by the renderer each frame.
        //
        // zoom is pixels-per-world-unit (higher = closer). Default 64 means
        // 1 world unit = 64 screen pixels.
        class Camera2D : public Component
        {
        public:
            float zoom = 64.0f;
            bool  isActive = false;
        };
    }
}