#pragma once

#include "../Node.hpp"
#include "../../AssetPack.hpp"
#include "../../renderer/SpriteBatcher.hpp"
#include "../../renderer/GlyphCache.hpp"
#include "../../renderer/Texture2D.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <string>
#include <memory>
#include <map>
#include <functional>

namespace Bokken
{
    namespace Renderer
    {
        class SpriteBatcher;
        class GlyphCache;
    }

    namespace Canvas
    {
        namespace Components
        {

            /**
             * Text component for the Canvas.
             *
             * After the GL refactor:
             *   - measurement (computeNode) still uses SDL_ttf directly via a tiny
             *     local font cache — it just needs glyph metrics, no rasterization.
             *   - drawing pulls glyph quads out of the engine-wide GlyphCache and
             *     emits them via the SpriteBatcher. No more SDL_Texture per Label.
             *
             * The engine wires a static GlyphCache* into Label before any draws.
             */
            class Label
            {
            public:
                Label(const std::string &text) : m_text(text) {}
                void setStyle(const SimpleStyleSheet &s) { m_style = s; }

                std::shared_ptr<Bokken::Canvas::Node> toNode();

                // Measurement (bottom-up). Sets node->layout.w/h from text metrics.
                static void computeNode(std::shared_ptr<Bokken::Canvas::Node> node, Bokken::AssetPack *assets);
                static void layoutNode(std::shared_ptr<Bokken::Canvas::Node> node);

                /**
                 * Draw the label using the SpriteBatcher and the shared GlyphCache.
                 * `layer` is forwarded to batcher quads for stacking; the engine
                 * Canvas walker increments it as it descends the tree.
                 */
                static void draw(Renderer::SpriteBatcher &batcher,
                                 std::shared_ptr<Bokken::Canvas::Node> node,
                                 Bokken::AssetPack *assets,
                                 int layer);

                // Legacy SDL_ttf measurement helper. Returns a font for this Label's
                // (path, size) — only used by computeNode for TTF_GetStringSize.
                static TTF_Font *get_font(const std::string &p, float s, Bokken::AssetPack *a);
                static void clear_font_cache();

                // Wired by the Renderer module so draw() can find the engine
                // GlyphCache without a per-call argument.
                static inline Bokken::Renderer::GlyphCache *s_glyphCache = nullptr;

            private:
                std::string m_text;
                SimpleStyleSheet m_style;
                // Measurement font cache. Keyed on "path:size". Owned by the class.
                static inline std::map<std::string, TTF_Font *> s_measureFonts;
            };

        }
    }
}
