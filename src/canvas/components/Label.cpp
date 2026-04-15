#include "Label.hpp"

namespace Bokken
{
    namespace Canvas
    {
        namespace Components
        {

            TTF_Font *Label::get_font(const std::string &path, float size, Bokken::AssetPack *assets, SDL_Renderer *renderer)
            {
                // Get DPI scale — defaults to 1.0 if renderer is null
                float scaleX = 1.0f, scaleY = 1.0f;
                if (renderer)
                    SDL_GetRenderScale(renderer, &scaleX, &scaleY);

                float scaledSize = size * scaleX;

                // Cache key now includes the scaled size to avoid HiDPI collisions
                std::string key = path + ":" + std::to_string((int)scaledSize);
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
                    // Load at physical pixel size, not logical size
                    TTF_Font *font = TTF_OpenFontIO(stream, true, scaledSize);
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

            void Label::computeNode(std::shared_ptr<Bokken::Canvas::Node> node, Bokken::AssetPack *assets)
            {
                const auto &s = node->style;
                float fSize = s.fontSize > 0.f ? s.fontSize : 16.f;

                // Pass nullptr for renderer — no scale at compute time, layout stays in logical pixels
                TTF_Font *font = Label::get_font(s.font, fSize, assets, nullptr);
                if (!font)
                    return;

                int textW, textH;
                if (TTF_GetStringSize(font, node->textContent.c_str(), 0, &textW, &textH))
                {
                    float pT = (s.paddingTop != 0) ? s.paddingTop : s.padding;
                    float pB = (s.paddingBottom != 0) ? s.paddingBottom : s.padding;
                    float pL = (s.paddingLeft != 0) ? s.paddingLeft : s.padding;
                    float pR = (s.paddingRight != 0) ? s.paddingRight : s.padding;

                    if (s.width <= 0 && !s.widthIsPercent)
                        node->layout.w = (float)textW + pL + pR;
                    if (s.height <= 0 && !s.heightIsPercent)
                        node->layout.h = (float)textH + pT + pB;
                }
            }

            void Label::layoutNode(std::shared_ptr<Bokken::Canvas::Node> node) {}

            std::shared_ptr<Bokken::Canvas::Node> Label::toNode()
            {
                auto node = std::make_shared<Bokken::Canvas::Node>("Label");
                node->textContent = m_text;
                node->style = m_style;

                node->onCompute = &computeNode;
                node->onLayout = &layoutNode;

                return node;
            }

            void Label::draw(SDL_Renderer *renderer, std::shared_ptr<Bokken::Canvas::Node> node, Bokken::AssetPack *assets)
            {
                if (!renderer || !node || node->textContent.empty())
                    return;

                const auto &s = node->style;

                // 1. Resolve Padding
                float pT = (s.paddingTop != 0) ? s.paddingTop : s.padding;
                float pL = (s.paddingLeft != 0) ? s.paddingLeft : s.padding;
                float pR = (s.paddingRight != 0) ? s.paddingRight : s.padding;
                float pB = (s.paddingBottom != 0) ? s.paddingBottom : s.padding;

                // 2. Get render scale for HiDPI compensation
                float scaleX = 1.0f, scaleY = 1.0f;
                SDL_GetRenderScale(renderer, &scaleX, &scaleY);

                // 3. Texture Management
                if (node->needsRepaint || node->texture == nullptr)
                {
                    if (node->texture)
                        SDL_DestroyTexture(node->texture);

                    float fSize = s.fontSize > 0.f ? s.fontSize : 16.f;

                    // Pass renderer so font is loaded at physical (scaled) size
                    TTF_Font *font = get_font(s.font, fSize, assets, renderer);
                    if (!font)
                        return;

                    SDL_Color textColor = {
                        static_cast<Uint8>((s.color >> 24) & 0xFF),
                        static_cast<Uint8>((s.color >> 16) & 0xFF),
                        static_cast<Uint8>((s.color >> 8) & 0xFF),
                        static_cast<Uint8>(s.color & 0xFF)};

                    SDL_Surface *surface = TTF_RenderText_Blended(font, node->textContent.c_str(), 0, textColor);
                    if (surface)
                    {
                        node->texture = SDL_CreateTextureFromSurface(renderer, surface);
                        SDL_DestroySurface(surface);
                        node->needsRepaint = false;
                    }
                }

                if (node->texture)
                {
                    float texW, texH;
                    SDL_GetTextureSize(node->texture, &texW, &texH);

                    // Convert physical texture size back to logical pixels for layout math
                    float logicalW = texW / scaleX;
                    float logicalH = texH / scaleY;

                    SDL_FRect drawRect = {node->layout.x, node->layout.y, logicalW, logicalH};

                    // 4. Horizontal Alignment
                    if (s.width > 0 || s.widthIsPercent)
                    {
                        if (s.alignItems == Align::Center)
                            drawRect.x = node->layout.x + (node->layout.w - logicalW) * 0.5f;
                        else if (s.alignItems == Align::End)
                            drawRect.x = node->layout.x + node->layout.w - logicalW - pR;
                        else
                            drawRect.x += pL;
                    }
                    else
                    {
                        drawRect.x += pL;
                    }

                    // 5. Vertical Alignment
                    if (s.height > 0 || s.heightIsPercent)
                        drawRect.y = node->layout.y + (node->layout.h - logicalH) * 0.5f;
                    else
                        drawRect.y += pT;

                    // 6. Apply Visual Scale (hover/active animations)
                    if (node->visualScale != 1.0f)
                    {
                        float cx = drawRect.x + logicalW * 0.5f;
                        float cy = drawRect.y + logicalH * 0.5f;
                        drawRect.w *= node->visualScale;
                        drawRect.h *= node->visualScale;
                        drawRect.x = cx - drawRect.w * 0.5f;
                        drawRect.y = cy - drawRect.h * 0.5f;
                    }

                    SDL_SetTextureAlphaMod(node->texture, static_cast<Uint8>(s.opacity * 255));
                    SDL_RenderTexture(renderer, node->texture, nullptr, &drawRect);
                }
            }
        }
    }
}