#pragma once

#include "Stage.hpp"
#include "RenderTarget.hpp"
#include "FrameContext.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <vector>
#include <memory>
#include <string>

namespace Bokken
{
    namespace Renderer
    {

        class SpriteBatcher;

        /**
         * Ordered list of stages. Owns a pair of HDR ping-pong targets and
         * rotates them between stages so each stage reads the previous
         * stage's output as a texture.
         *
         * The Pipeline does NOT own the SpriteBatcher — that's per-frame
         * shared state passed via FrameContext.
         *
         * The last stage's output is what eventually lands on the default
         * framebuffer. By convention the final stage is "composite" — it
         * reads the chain's output and blits to the screen with whatever
         * tone-mapping / gamma is wanted.
         *
         * JS-facing operations (add / remove / move / setEnabled) are
         * exposed as plain methods; the bokken/renderer scripting module
         * just forwards to them.
         */
        class Pipeline
        {
        public:
            Pipeline() = default;

            /** Allocate ping-pong targets. Call after a GL context exists. */
            bool init(int width, int height);

            /** Recreate ping-pong targets at a new size. Cheap if unchanged. */
            bool resize(int width, int height);

            /** Render a frame. Walks stages in order, rotating targets,
             *  finally leaves the last stage's output bound for whatever
             *  composite step the caller wants. */
            void render(SpriteBatcher *batcher, float dt);

            // ── Stage management ──
            void addStage(std::unique_ptr<Stage> stage);
            bool removeStage(const std::string &name);
            bool moveStage(const std::string &name, int newIndex);
            Stage *findStage(const std::string &name);
            const std::vector<std::unique_ptr<Stage>> &stages() const { return m_stages; }

            // The final output target after render(). Caller (Renderer)
            // composites this into the default framebuffer.
            const RenderTarget *finalOutput() const { return m_lastOutput; }

            int width() const { return m_width; }
            int height() const { return m_height; }

        private:
            std::vector<std::unique_ptr<Stage>> m_stages;
            // Two HDR scratch targets, rotated between stages.
            RenderTarget m_targetA;
            RenderTarget m_targetB;
            const RenderTarget *m_lastOutput = nullptr;
            int m_width = 0;
            int m_height = 0;
        };

    }
}
