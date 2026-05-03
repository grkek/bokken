#include "GlyphCache.hpp"

namespace Bokken
{
    namespace Renderer
    {

        static constexpr int kInitialAtlasSize = 1024;
        static constexpr int kMaxAtlasSize = 4096;

        GlyphCache::~GlyphCache()
        {
            for (auto &kv : m_fonts)
            {
                if (kv.second.font)
                    TTF_CloseFont(kv.second.font);
            }
        }

        std::string GlyphCache::fontKey(const std::string &path, float size) const
        {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "@%d", (int)(size + 0.5f));
            return path + buf;
        }

        bool GlyphCache::init()
        {
            if (!m_atlas.create(kInitialAtlasSize, kInitialAtlasSize,
                                TextureFormat::R8,
                                TextureFilter::Linear, TextureWrap::Clamp))
            {
                SDL_LogError(SDL_LOG_CATEGORY_RENDER, "[GlyphCache] atlas creation failed");
                return false;
            }
            m_atlasW = kInitialAtlasSize;
            m_atlasH = kInitialAtlasSize;

            std::vector<uint8_t> zeros(m_atlasW * m_atlasH, 0);
            m_atlas.upload(0, 0, m_atlasW, m_atlasH, zeros.data());

            m_penX = m_penY = m_rowHeight = 0;
            return true;
        }

        TTF_Font *GlyphCache::getFont(const std::string &path, float size, AssetPack *assets)
        {
            const std::string key = fontKey(path, size);
            auto it = m_fonts.find(key);
            if (it != m_fonts.end())
                return it->second.font;

            if (!assets)
                return nullptr;

            SDL_IOStream *io = assets->openIOStream(path);
            if (!io)
            {
                SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                             "[GlyphCache] font not in asset pack: %s", path.c_str());
                return nullptr;
            }
            TTF_Font *font = TTF_OpenFontIO(io, true, (int)(size + 0.5f));
            if (!font)
            {
                SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                             "[GlyphCache] TTF_OpenFontIO failed: %s", SDL_GetError());
                return nullptr;
            }

            FontEntry entry;
            entry.font = font;
            m_fonts.emplace(key, std::move(entry));
            return font;
        }

        bool GlyphCache::growAtlasIfNeeded(int needW, int needH)
        {
            if (m_penX + needW <= m_atlasW && m_penY + needH <= m_atlasH)
                return true;

            if (m_penY + m_rowHeight + needH <= m_atlasH)
            {
                m_penX = 0;
                m_penY += m_rowHeight;
                m_rowHeight = 0;
                return true;
            }

            int newW = m_atlasW;
            int newH = m_atlasH;
            while ((newW < kMaxAtlasSize || newH < kMaxAtlasSize) &&
                   (m_penY + m_rowHeight + needH > newH))
            {
                if (newW < kMaxAtlasSize)
                    newW *= 2;
                if (newH < kMaxAtlasSize)
                    newH *= 2;
            }
            if (newW > kMaxAtlasSize || newH > kMaxAtlasSize)
            {
                SDL_LogError(SDL_LOG_CATEGORY_RENDER, "[GlyphCache] atlas full");
                return false;
            }

            Texture2D fresh;
            if (!fresh.create(newW, newH, TextureFormat::R8,
                              TextureFilter::Linear, TextureWrap::Clamp))
                return false;
            std::vector<uint8_t> zeros(newW * newH, 0);
            fresh.upload(0, 0, newW, newH, zeros.data());

            m_atlas = std::move(fresh);
            m_atlasW = newW;
            m_atlasH = newH;
            m_penX = m_penY = m_rowHeight = 0;
            for (auto &kv : m_fonts)
                kv.second.glyphs.clear();
            return true;
        }

        bool GlyphCache::rasterizeInto(TTF_Font *font, uint32_t codepoint, Glyph &outGlyph)
        {
            if (!font)
                return false;

            int minx, maxx, miny, maxy, advance;
            TTF_GetGlyphMetrics(font, codepoint, &minx, &maxx, &miny, &maxy, &advance);

            SDL_Color white = {255, 255, 255, 255};
            SDL_Surface *raw = TTF_RenderGlyph_Blended(font, codepoint, white);
            if (!raw)
            {
                SDL_LogWarn(SDL_LOG_CATEGORY_RENDER,
                            "[GlyphCache] failed to rasterize U+%04X: %s",
                            codepoint, SDL_GetError());
                return false;
            }

            const int surfW = raw->w;
            const int surfH = raw->h;
            if (surfW == 0 || surfH == 0)
            {
                SDL_DestroySurface(raw);
                outGlyph = {0, 0, 0, 0, 0, 0, advance, 0, 0};
                return true;
            }

            // Convert to RGBA32 for reliable byte layout (R=0, G=1, B=2, A=3).
            SDL_Surface *surf = (raw->format == SDL_PIXELFORMAT_RGBA32)
                                    ? raw
                                    : SDL_ConvertSurface(raw, SDL_PIXELFORMAT_RGBA32);
            if (!surf)
            {
                SDL_LogWarn(SDL_LOG_CATEGORY_RENDER,
                            "[GlyphCache] SDL_ConvertSurface failed for U+%04X: %s",
                            codepoint, SDL_GetError());
                SDL_DestroySurface(raw);
                return false;
            }

            // Tight-crop the glyph bitmap to actual ink pixels.
            //
            // SDL_ttf's TTF_RenderGlyph_Blended produces a surface covering
            // the full line height with the glyph positioned internally at
            // row (ascent - maxy). Storing this full surface wastes atlas
            // space and — critically — makes the draw code position every
            // glyph at the same Y with the ink floating inside, which breaks
            // visual centering within fixed-size containers.
            //
            // By cropping to the tight ink bounding box and using per-glyph
            // bearingX/bearingY, each glyph quad covers exactly the visible
            // pixels. The draw loop places them at:
            //   gx = penX + bearingX
            //   gy = baseline - bearingY
            //
            // With bearingX = minx and bearingY = maxy, this gives:
            //   gx = penX + minx        (ink starts at the correct x)
            //   gy = baseline - maxy    (ink top at the correct y)
            //
            // This is how CryEngine, id Tech, and stb_truetype-based
            // renderers all work: tightly-packed atlas + per-glyph offsets.

            const uint8_t *src = static_cast<const uint8_t *>(surf->pixels);
            const int pitch = surf->pitch;

            // Find the tight bounding box by scanning for nonzero alpha.
            int cropTop = surfH, cropBottom = -1;
            int cropLeft = surfW, cropRight = -1;

            for (int y = 0; y < surfH; ++y)
            {
                const uint8_t *row = src + y * pitch;
                for (int x = 0; x < surfW; ++x)
                {
                    if (row[x * 4 + 3] > 0)
                    {
                        if (y < cropTop)
                            cropTop = y;
                        if (y > cropBottom)
                            cropBottom = y;
                        if (x < cropLeft)
                            cropLeft = x;
                        if (x > cropRight)
                            cropRight = x;
                    }
                }
            }

            if (cropBottom < cropTop || cropRight < cropLeft)
            {
                // No visible pixels — treat as whitespace.
                if (surf != raw)
                    SDL_DestroySurface(surf);
                SDL_DestroySurface(raw);
                outGlyph = {0, 0, 0, 0, 0, 0, advance, 0, 0};
                return true;
            }

            const int cropW = cropRight - cropLeft + 1;
            const int cropH = cropBottom - cropTop + 1;

            if (!growAtlasIfNeeded(cropW + 1, cropH + 1))
            {
                if (surf != raw)
                    SDL_DestroySurface(surf);
                SDL_DestroySurface(raw);
                return false;
            }

            // Extract the cropped R8 region.
            std::vector<uint8_t> r8(static_cast<size_t>(cropW) * cropH);
            for (int y = 0; y < cropH; ++y)
            {
                const uint8_t *row = src + (cropTop + y) * pitch;
                for (int x = 0; x < cropW; ++x)
                {
                    r8[y * cropW + x] = row[(cropLeft + x) * 4 + 3];
                }
            }

            m_atlas.upload(m_penX, m_penY, cropW, cropH, r8.data());

            outGlyph.u0 = m_penX;
            outGlyph.v0 = m_penY;
            outGlyph.u1 = m_penX + cropW;
            outGlyph.v1 = m_penY + cropH;
            outGlyph.width = cropW;
            outGlyph.height = cropH;

            // Per-glyph bearing from font metrics.
            //
            // bearingX = minx: horizontal offset from pen to ink left edge.
            //
            // bearingY: we need the distance from baseline DOWN to the
            // ink's top edge, but expressed so that:
            //   gy = baseline - bearingY = ink top row on screen
            //
            // The ink's top row on screen should be at (baseline - maxy)
            // because maxy is the glyph's ascent above the baseline.
            //
            // So: baseline - bearingY = baseline - maxy  →  bearingY = maxy.
            //
            // However, SDL_ttf's surface places the ink at row (ascent - maxy)
            // from the surface top. Our crop removed rows above cropTop, so
            // the relationship between the crop offset and the metrics is:
            //   cropTop = ascent - maxy  (where ink starts in the surface)
            //
            // Using bearingY = maxy directly from metrics is correct and
            // does NOT depend on surface layout — it's a pure font metric.
            outGlyph.bearingX = minx;
            outGlyph.bearingY = maxy;
            outGlyph.advance = advance;

            m_penX += cropW + 1;
            if (cropH + 1 > m_rowHeight)
                m_rowHeight = cropH + 1;

            if (surf != raw)
                SDL_DestroySurface(surf);
            SDL_DestroySurface(raw);
            return true;
        }

        const GlyphCache::Glyph *GlyphCache::getGlyph(const std::string &fontPath, float size,
                                                      AssetPack *assets, uint32_t codepoint)
        {
            const std::string key = fontKey(fontPath, size);
            auto fIt = m_fonts.find(key);
            if (fIt == m_fonts.end())
            {
                if (!getFont(fontPath, size, assets))
                    return nullptr;
                fIt = m_fonts.find(key);
                if (fIt == m_fonts.end())
                    return nullptr;
            }

            auto gIt = fIt->second.glyphs.find(codepoint);
            if (gIt != fIt->second.glyphs.end())
                return &gIt->second;

            Glyph g{};
            if (!rasterizeInto(fIt->second.font, codepoint, g))
                return nullptr;
            auto [insIt, _] = fIt->second.glyphs.emplace(codepoint, g);
            return &insIt->second;
        }

    }
}