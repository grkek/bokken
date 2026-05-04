#include "TextureCache.hpp"

namespace Bokken
{
    namespace Renderer
    {

        const Texture2D *TextureCache::load(const std::string &virtualPath,
                                            AssetPack *assets,
                                            TextureFilter filter,
                                            TextureWrap wrap)
        {
            // Return cached if already loaded.
            auto it = m_textures.find(virtualPath);
            if (it != m_textures.end())
                return it->second.get();

            if (!assets || !assets->exists(virtualPath))
            {
                SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                             "[TextureCache] asset not found: %s", virtualPath.c_str());
                return nullptr;
            }

            // Load through the PhysFS IOStream bridge so we never touch
            // the real filesystem — everything comes from asset packs.
            SDL_IOStream *io = assets->openIOStream(virtualPath);
            if (!io)
            {
                SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                             "[TextureCache] failed to open IOStream: %s", virtualPath.c_str());
                return nullptr;
            }

            // SDL_image closes the IOStream for us when closeio is true.
            SDL_Surface *raw = IMG_Load_IO(io, true);
            if (!raw)
            {
                SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                             "[TextureCache] IMG_Load_IO failed for '%s': %s",
                             virtualPath.c_str(), SDL_GetError());
                return nullptr;
            }

            // Convert to RGBA32 for a consistent upload path.
            SDL_Surface *rgba = (raw->format == SDL_PIXELFORMAT_RGBA32)
                                    ? raw
                                    : SDL_ConvertSurface(raw, SDL_PIXELFORMAT_RGBA32);
            if (!rgba)
            {
                SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                             "[TextureCache] surface conversion failed for '%s': %s",
                             virtualPath.c_str(), SDL_GetError());
                SDL_DestroySurface(raw);
                return nullptr;
            }

            auto tex = std::make_unique<Texture2D>();
            bool ok = tex->uploadFull(rgba->w, rgba->h, TextureFormat::RGBA8,
                                      rgba->pixels, filter, wrap);

            if (rgba != raw)
                SDL_DestroySurface(rgba);
            SDL_DestroySurface(raw);

            if (!ok)
            {
                SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                             "[TextureCache] GL upload failed for '%s'", virtualPath.c_str());
                return nullptr;
            }

            Texture2D *ptr = tex.get();
            m_textures.emplace(virtualPath, std::move(tex));

            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "[TextureCache] loaded '%s' (%dx%d)\n",
                    virtualPath.c_str(), ptr->width(), ptr->height());
            return ptr;
        }

        const Texture2D *TextureCache::get(const std::string &virtualPath) const
        {
            auto it = m_textures.find(virtualPath);
            return (it != m_textures.end()) ? it->second.get() : nullptr;
        }

        void TextureCache::unload(const std::string &virtualPath)
        {
            // Remove any regions that reference this texture.
            auto texIt = m_textures.find(virtualPath);
            if (texIt != m_textures.end())
            {
                const Texture2D *tex = texIt->second.get();
                for (auto it = m_regions.begin(); it != m_regions.end();)
                {
                    if (it->second.texture == tex)
                        it = m_regions.erase(it);
                    else
                        ++it;
                }
                m_textures.erase(texIt);
            }
        }

        void TextureCache::clear()
        {
            m_regions.clear();
            m_textures.clear();
        }

        void TextureCache::defineRegion(const std::string &name,
                                        const std::string &texturePath,
                                        int x, int y, int w, int h)
        {
            const Texture2D *tex = get(texturePath);
            if (!tex)
            {
                SDL_LogWarn(SDL_LOG_CATEGORY_RENDER,
                            "[TextureCache] defineRegion: texture '%s' not loaded",
                            texturePath.c_str());
                return;
            }

            float tw = static_cast<float>(tex->width());
            float th = static_cast<float>(tex->height());

            TextureRegion r;
            r.texture = tex;
            r.x = x;
            r.y = y;
            r.w = w;
            r.h = h;
            r.u0 = x / tw;
            r.v0 = y / th;
            r.u1 = (x + w) / tw;
            r.v1 = (y + h) / th;

            m_regions[name] = r;
        }

        int TextureCache::defineGrid(const std::string &prefix,
                                     const std::string &texturePath,
                                     int frameW, int frameH,
                                     int count,
                                     int offsetX, int offsetY,
                                     int paddingX, int paddingY)
        {
            const Texture2D *tex = get(texturePath);
            if (!tex)
            {
                SDL_LogWarn(SDL_LOG_CATEGORY_RENDER,
                            "[TextureCache] defineGrid: texture '%s' not loaded",
                            texturePath.c_str());
                return 0;
            }

            int cols = (tex->width() - offsetX + paddingX) / (frameW + paddingX);
            int rows = (tex->height() - offsetY + paddingY) / (frameH + paddingY);
            int total = cols * rows;

            if (count > 0 && count < total)
                total = count;

            float tw = static_cast<float>(tex->width());
            float th = static_cast<float>(tex->height());

            int created = 0;
            for (int i = 0; i < total; ++i)
            {
                int col = i % cols;
                int row = i / cols;
                int x = offsetX + col * (frameW + paddingX);
                int y = offsetY + row * (frameH + paddingY);

                // Bounds check — don't create regions that overflow the texture.
                if (x + frameW > tex->width() || y + frameH > tex->height())
                    continue;

                TextureRegion r;
                r.texture = tex;
                r.x = x;
                r.y = y;
                r.w = frameW;
                r.h = frameH;
                r.u0 = x / tw;
                r.v0 = y / th;
                r.u1 = (x + frameW) / tw;
                r.v1 = (y + frameH) / th;

                std::string name = prefix + "_" + std::to_string(i);
                m_regions[name] = r;
                ++created;
            }

            return created;
        }

        const TextureRegion *TextureCache::region(const std::string &name) const
        {
            auto it = m_regions.find(name);
            return (it != m_regions.end()) ? &it->second : nullptr;
        }

        TextureRegion TextureCache::fullRegion(const std::string &texturePath) const
        {
            TextureRegion r;
            const Texture2D *tex = get(texturePath);
            if (!tex)
                return r;

            r.texture = tex;
            r.x = 0;
            r.y = 0;
            r.w = tex->width();
            r.h = tex->height();
            r.u0 = 0.0f;
            r.v0 = 0.0f;
            r.u1 = 1.0f;
            r.v1 = 1.0f;
            return r;
        }

    }
}