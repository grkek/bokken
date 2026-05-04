#pragma once

#include "Shader.hpp"
#include "Texture2D.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <vector>
#include <cstdint>
#include <cstring>

namespace Bokken
{
    namespace Renderer
    {

        /**
         * Blend mode for individual quads.
         *
         * Alpha is the standard porter-duff over (src * srcA + dst * (1-srcA)).
         * Additive is src * srcA + dst * 1 — used for fire, explosions, magic
         * effects, and anything that should brighten what's behind it.
         * Screen is 1 - (1-src) * (1-dst) — subtler than additive, good for
         * glows that shouldn't blow out to white.
         */
        enum class BlendMode : uint8_t
        {
            Alpha,
            Additive,
            Screen,
        };

        /**
         * Batched 2D quad renderer.
         *
         * The single hottest code path in the engine. Everything 2D — sprite
         * quads, Canvas <View> rectangles, Canvas glyphs — funnels through
         * here. Calls accumulate quads into CPU-side vertex buffers; `flush()`
         * uploads and issues draw calls grouped by texture and blend mode.
         *
         * Layer ordering:
         *   Each push takes a `layer` (smaller = drawn first / behind).
         *   Within a layer, quads are sorted by blend mode then texture id
         *   to minimise state changes.
         *
         * Coordinate space:
         *   We render in pixel space — the projection matrix is set up so
         *   x ∈ [0, viewportWidth], y ∈ [0, viewportHeight] with y growing
         *   downward (screen-style, matching Canvas layout). Caller never
         *   needs to know about NDC.
         *
         * The "white texture" trick:
         *   Solid-color quads (Canvas Views, debug rects) are drawn against
         *   a 1×1 white texture so they share the same shader as textured
         *   quads. One pipeline, one shader, perfect batching.
         *
         * Triple-buffered VBO:
         *   The VBO is divided into three equally-sized segments. Each frame
         *   rotates to the next segment and writes via glBufferSubData into
         *   the region the GPU finished reading two frames ago. This avoids
         *   the implicit sync that glBufferData (even with orphan hint)
         *   causes on some drivers, and is the standard approach for GL 3.3
         *   where persistent mapping isn't available.
         */
        class SpriteBatcher
        {
        public:
            struct Quad
            {
                float x, y;               // top-left in pixels (or center when rotated)
                float w, h;               // size in pixels
                float u0, v0;             // top-left UV
                float u1, v1;             // bottom-right UV
                uint32_t rgba;            // packed RGBA8 — endianness-safe via shader unpack
                const Texture2D *texture; // null = solid color (uses white texture)
                int layer;                // sort key: smaller = behind
                float rotation;           // radians, counter-clockwise around center of quad
                BlendMode blend;          // per-quad blend mode
            };

            SpriteBatcher() = default;
            ~SpriteBatcher();

            bool init();

            /** Begin a frame. width/height are the viewport in pixels. */
            void begin(int viewportWidth, int viewportHeight);

            /** Push a textured quad. UVs in [0,1]. */
            void drawTextured(const Texture2D *tex,
                              float x, float y, float w, float h,
                              float u0, float v0, float u1, float v1,
                              uint32_t rgba = 0xFFFFFFFFu, int layer = 0,
                              BlendMode blend = BlendMode::Alpha);

            /** Solid color rectangle. RGBA packed as 0xRRGGBBAA. */
            void drawRect(float x, float y, float w, float h,
                          uint32_t rgba, int layer = 0,
                          BlendMode blend = BlendMode::Alpha);

            /** Solid color rectangle with rotation (radians) around its center. */
            void drawRotatedRect(float cx, float cy, float w, float h,
                                 float rotationRad, uint32_t rgba, int layer = 0,
                                 BlendMode blend = BlendMode::Alpha);

            /** Issue draw calls. Safe to call multiple times per frame. */
            void flush();

            /** Stats from the last flush, for HUD/debug. */
            struct Stats
            {
                int quadCount = 0;
                int drawCallCount = 0;
                int textureBindCount = 0;
                int blendSwitchCount = 0;
            };
            const Stats &lastFrameStats() const { return m_stats; }

        private:
            // Per-vertex layout: pos.xy, uv.xy, color.rgba (float)
            struct Vertex
            {
                float x, y;
                float u, v;
                float r, g, b, a;
            };

            std::vector<Quad> m_quads;
            std::vector<Vertex> m_verts;
            std::vector<uint32_t> m_indices;

            Shader m_shader;
            Texture2D m_whiteTex;
            GLuint m_vao = 0;
            GLuint m_vbo = 0;
            GLuint m_ibo = 0;

            int m_viewportW = 0;
            int m_viewportH = 0;
            Stats m_stats;

            // Triple buffering state. The VBO and IBO are each allocated
            // at 3× the working capacity. m_bufferSegment rotates 0→1→2→0
            // each frame so we never write into a region the GPU is still
            // reading from a previous frame.
            static constexpr int k_bufferSegments = 3;
            int m_bufferSegment = 0;
            size_t m_vboSegmentSize = 0;   // bytes per segment
            size_t m_iboSegmentSize = 0;
            size_t m_vboCapacityVerts = 0;  // verts per segment
            size_t m_iboCapacityInds = 0;   // indices per segment

            void issueBatch(const Texture2D *tex, BlendMode blend,
                            size_t firstQuad, size_t count);
            void ensureBufferCapacity(size_t totalVerts, size_t totalIndices);
            void applyBlendMode(BlendMode mode);
        };

    }
}