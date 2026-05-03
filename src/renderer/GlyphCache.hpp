#pragma once

#include "Texture2D.hpp"
#include "../AssetPack.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "gl/GL.hpp"

#include <unordered_map>
#include <string>
#include <cstdint>
#include <memory>
#include <cstring>
#include <vector>

namespace Bokken
{
    namespace Renderer
    {

        /**
         * Glyph atlas backed by SDL_ttf.
         *
         * For each (font path, font size) we maintain:
         *   - an open TTF_Font
         *   - an R8 GL atlas texture (single-channel coverage)
         *   - a per-codepoint glyph table with UVs and metrics
         *
         * Glyphs are rasterized lazily on first use. The atlas grows in
         * a "shelf" pattern: rows of fixed height, glyphs packed left-to-right.
         * When a row fills, we advance to the next row. When the atlas fills,
         * we double its size and re-rasterize everything (rare in practice).
         *
         * We don't try to be clever about Unicode — every codepoint requested
         * gets rasterized once and kept. For typical western UI workloads
         * the atlas stays small (<256KB).
         *
         * This is the bridge between SDL_ttf's classic SDL_Surface output
         * and our GL pipeline. Replacing SDL_ttf with FreeType or stb_truetype
         * is a swap-in change here later — the public API doesn't depend on it.
         */
        class GlyphCache
        {
        public:
            struct Glyph
            {
                // Position within the atlas (pixels).
                int u0, v0, u1, v1;
                // Sub-pixel offsets and advance from the font.
                int bearingX, bearingY;
                int advance;
                int width, height;
            };

            GlyphCache() = default;
            ~GlyphCache();

            bool init();

            /**
             * Get (and cache) a font handle.
             *
             * `path` is virtual — resolved against the asset pack. Sizes are in
             * pixels (already DPI-scaled by the caller). Returns nullptr on fail.
             */
            TTF_Font *getFont(const std::string &path, float size, AssetPack *assets);

            /**
             * Get the glyph entry for a (font, size, codepoint), rasterizing
             * into the atlas on first miss. Returns nullptr if the font is
             * unavailable or the glyph can't be rendered.
             */
            const Glyph *getGlyph(const std::string &fontPath, float size, AssetPack *assets,
                                  uint32_t codepoint);

            /** The atlas texture — bound by SpriteBatcher when drawing glyphs. */
            const Texture2D *atlas() const { return &m_atlas; }
            Texture2D *atlas() { return &m_atlas; }

        private:
            // Key = font path + "@" + integer pixel size.
            struct FontEntry
            {
                TTF_Font *font = nullptr;
                // Each (codepoint) → Glyph for this font/size.
                std::unordered_map<uint32_t, Glyph> glyphs;
            };

            Texture2D m_atlas;
            int m_atlasW = 0;
            int m_atlasH = 0;
            // Shelf-allocator state.
            int m_penX = 0;
            int m_penY = 0;
            int m_rowHeight = 0;

            std::unordered_map<std::string, FontEntry> m_fonts;

            std::string fontKey(const std::string &path, float size) const;
            bool growAtlasIfNeeded(int needW, int needH);
            bool rasterizeInto(TTF_Font *font, uint32_t codepoint, Glyph &outGlyph);
        };

    }
}
