#include "Label.hpp"

namespace Bokken
{
    namespace Canvas
    {
        namespace Components
        {

            TTF_Font *Label::get_font(const std::string &path, float size, Bokken::AssetPack *assets)
            {
                std::string key = path + ":" + std::to_string((int)size);
                if (s_font_cache.count(key))
                    return s_font_cache[key];

                if (!assets)
                {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[Label] AssetPack not initialized.");
                    return nullptr;
                }

                std::string targetPath = path;
                if (targetPath.empty() || !assets->exists(targetPath))
                    targetPath = "fonts/default.ttf";

                if (!assets->exists(targetPath))
                {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[Label] Font not found: %s", targetPath.c_str());
                    return nullptr;
                }

                SDL_IOStream *stream = assets->openIOStream(targetPath);
                if (stream)
                {
                    TTF_Font *font = TTF_OpenFontIO(stream, true, size);
                    if (font)
                    {
                        s_font_cache[key] = font;
                        return font;
                    }
                }

                SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "[Label] Failed to load font '%s': %s",
                             targetPath.c_str(), SDL_GetError());
                return nullptr;
            }

            void Label::clear_font_cache()
            {
                for (auto &[key, font] : s_font_cache)
                    TTF_CloseFont(font);
                s_font_cache.clear();
            }

            std::shared_ptr<Bokken::Canvas::Node> Label::toNode()
            {
                auto node = std::make_shared<Bokken::Canvas::Node>("Label");
                node->textContent = m_text;
                node->style = m_style;

                // We determine the intrinsic size of the text based on the font.
                node->onCompute = [](std::shared_ptr<Bokken::Canvas::Node> n, Bokken::AssetPack *a)
                {
                    float fSize = n->style.fontSize > 0.f ? n->style.fontSize : 16.f;
                    TTF_Font *font = Bokken::Canvas::Components::Label::get_font(n->style.font, fSize, a);

                    if (font)
                    {
                        int w, h;
                        if (TTF_GetStringSize(font, n->textContent.c_str(), 0, &w, &h))
                        {
                            float textW = (float)w;
                            float textH = (float)h;

                            // Resolve Width: Percent > Fixed > Content
                            if (n->style.widthIsPercent)
                            {
                                n->layout.w = n->layout.w * (n->style.width / 100.0f);
                            }
                            else if (n->style.width > 0)
                            {
                                n->layout.w = n->style.width;
                            }
                            else
                            {
                                n->layout.w = textW;
                            }

                            // Resolve Height: Percent > Fixed > Content
                            if (n->style.heightIsPercent)
                            {
                                n->layout.h = n->layout.h * (n->style.height / 100.0f);
                            }
                            else if (n->style.height > 0)
                            {
                                n->layout.h = n->style.height;
                            }
                            else
                            {
                                n->layout.h = textH;
                            }
                        }
                    }
                };

                // For a Label, we don't have children to position, but we should
                // handle any internal offset logic here if we ever add padding/margins.
                node->onLayout = [](std::shared_ptr<Bokken::Canvas::Node> n)
                {
                    // Labels are usually leaf nodes, so we just finish the pass here.
                    // If we ever support inline icons inside Labels, we'd position them here.
                };

                return node;
            }

            void Label::draw(SDL_Renderer *renderer, std::shared_ptr<Bokken::Canvas::Node> node, Bokken::AssetPack *assets)
            {
                if (!renderer || !node || node->textContent.empty())
                    return;

                // 1. Use the Cache: If texture is valid and we don't need a repaint, just blit and leave.
                if (!node->needsRepaint && node->texture != nullptr)
                {
                    SDL_FRect rect = {node->layout.x, node->layout.y, node->layout.w, node->layout.h};
                    SDL_RenderTexture(renderer, node->texture, nullptr, &rect);
                    return;
                }

                // 2. Dirty/New Texture Path: Clean up old texture if it exists
                if (node->texture)
                {
                    SDL_DestroyTexture(node->texture);
                    node->texture = nullptr;
                }

                // 3. Render new Surface (Software)
                float fSize = node->style.fontSize > 0.f ? node->style.fontSize : 16.f;
                TTF_Font *font = get_font(node->style.font, fSize, assets);

                if (!font)
                {
                    SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "[Label Draw] Font is NULL for text: %s", node->textContent.c_str());
                    return;
                }

                SDL_Color white = {255, 255, 255, 255};
                SDL_Surface *surface = TTF_RenderText_Blended(font, node->textContent.c_str(), 0, white);
                if (!surface)
                {
                    SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "[Label Draw] Surface creation failed: %s", SDL_GetError());
                    return;
                }

                // 4. Create new Texture (Upload to GPU)
                node->texture = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_DestroySurface(surface); // Clean up surface immediately after texture creation

                // 5. Final Render
                if (node->texture)
                {
                    node->needsRepaint = false; // Mark as clean so we don't do this again!

                    SDL_FRect rect = {node->layout.x, node->layout.y, node->layout.w, node->layout.h};

                    if (rect.w <= 0 || rect.h <= 0)
                    {
                        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "[Label Draw] Zero size detected for: %s", node->textContent.c_str());
                    }

                    SDL_RenderTexture(renderer, node->texture, nullptr, &rect);
                }
            }
        }
    }
}