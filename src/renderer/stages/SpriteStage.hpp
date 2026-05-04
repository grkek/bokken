#pragma once

#include "../Stage.hpp"

#include "glad/gl.h"

namespace Bokken
{
    namespace Renderer
    {

        /**
         * Draws all queued 2D content into its output target.
         *
         * This is the default scene stage for 2D games. It expects the
         * SpriteBatcher to have been populated *before* the pipeline runs
         * (i.e. user code calls `renderer.batcher().drawRect(...)` between
         * beginFrame() and endFrame()).
         *
         * Clears the output target to a configurable clear color, then
         * flushes the batcher.
         */
        class SpriteStage : public Stage
        {
        public:
            explicit SpriteStage(std::string name = "scene")
                : Stage(std::move(name), Kind::Scene) {}

            // Clear color — applied to the output target before drawing.
            // Use 0 alpha if you want the next stage to see "no scene drawn here".
            float clearR = 0.075f, clearG = 0.090f, clearB = 0.105f, clearA = 1.0f;

            void execute(const FrameContext &ctx) override;
        };

    }
}
