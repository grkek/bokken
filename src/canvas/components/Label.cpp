#include "Label.hpp"

namespace Bokken
{
    namespace Canvas
    {
        namespace Components
        {

            static size_t decodeUTF8(const char *p, const char *end, uint32_t *cp)
            {
                if (p >= end)
                {
                    *cp = 0;
                    return 0;
                }
                const unsigned char c = (unsigned char)p[0];
                if (c < 0x80)
                {
                    *cp = c;
                    return 1;
                }
                if ((c & 0xE0) == 0xC0 && p + 1 < end)
                {
                    *cp = ((c & 0x1F) << 6) | ((unsigned char)p[1] & 0x3F);
                    return 2;
                }
                if ((c & 0xF0) == 0xE0 && p + 2 < end)
                {
                    *cp = ((c & 0x0F) << 12) | (((unsigned char)p[1] & 0x3F) << 6) | ((unsigned char)p[2] & 0x3F);
                    return 3;
                }
                if ((c & 0xF8) == 0xF0 && p + 3 < end)
                {
                    *cp = ((c & 0x07) << 18) | (((unsigned char)p[1] & 0x3F) << 12) | (((unsigned char)p[2] & 0x3F) << 6) | ((unsigned char)p[3] & 0x3F);
                    return 4;
                }
                *cp = '?';
                return 1;
            }

            TTF_Font *Label::get_font(const std::string &path, float size, AssetPack *assets)
            {
                const int px = (int)(size + 0.5f);
                std::string key = path + ":" + std::to_string(px);
                auto it = s_measureFonts.find(key);
                if (it != s_measureFonts.end())
                    return it->second;

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
                if (!stream)
                    return nullptr;
                TTF_Font *font = TTF_OpenFontIO(stream, true, px);
                if (!font)
                {
                    SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "[Label] TTF_OpenFontIO failed: %s", SDL_GetError());
                    return nullptr;
                }
                s_measureFonts[key] = font;
                return font;
            }

            void Label::clear_font_cache()
            {
                for (auto &kv : s_measureFonts)
                    if (kv.second)
                        TTF_CloseFont(kv.second);
                s_measureFonts.clear();
            }

            // Measurement uses advance-based width and font line height.
            // The advance width is correct for layout: it includes side
            // bearings and produces proper inter-label spacing. The line
            // height ensures consistent vertical sizing across labels.
            //
            // The parent View centers this layout box. With the GlyphCache
            // now producing tightly-cropped bitmaps and per-glyph bearings,
            // the ink is positioned correctly within the box by the draw
            // loop, so no ink-correction hacks are needed here.

            void Label::computeNode(std::shared_ptr<Bokken::Canvas::Node> node, AssetPack *assets)
            {
                const auto &s = node->style;
                const float fSize = s.fontSize > 0.f ? s.fontSize : 16.f;
                const std::string fontPath = s.font.empty() ? std::string("fonts/default.ttf") : s.font;

                TTF_Font *font = Label::get_font(fontPath, fSize, assets);
                if (!font)
                    return;

                int textW = 0, textH = 0;
                TTF_GetStringSize(font, node->textContent.c_str(), 0, &textW, &textH);

                const int lineHeight = TTF_GetFontHeight(font);

                const float pT = resolveSide(s.paddingTop, s.padding);
                const float pB = resolveSide(s.paddingBottom, s.padding);
                const float pL = resolveSide(s.paddingLeft, s.padding);
                const float pR = resolveSide(s.paddingRight, s.padding);

                node->measuredW = (float)textW;
                node->measuredH = (float)lineHeight;

                if (s.width <= 0 && !s.widthIsPercent)
                    node->layout.w = (float)textW + pL + pR;
                if (s.height <= 0 && !s.heightIsPercent)
                    node->layout.h = (float)lineHeight + pT + pB;
            }

            void Label::layoutNode(std::shared_ptr<Bokken::Canvas::Node> /*node*/) {}

            std::shared_ptr<Bokken::Canvas::Node> Label::toNode()
            {
                auto node = std::make_shared<Bokken::Canvas::Node>("Label");
                node->textContent = m_text;
                node->style = m_style;
                node->onCompute = &computeNode;
                node->onLayout = &layoutNode;
                return node;
            }

            // Drawing.
            //
            // With tightly-cropped glyph bitmaps from the GlyphCache, the
            // draw loop is straightforward:
            //
            //   gx = penX + bearingX        (bearingX = minx from metrics)
            //   gy = baseline - bearingY    (bearingY = maxy from metrics)
            //
            // Each glyph quad covers exactly the visible ink pixels.
            // The baseline is computed from the layout box and font ascent.
            //
            // Pixel-snapping is applied when not animating (scale == 1.0)
            // to prevent sub-pixel wobble from bilinear filtering.

            void Label::draw(Renderer::SpriteBatcher &batcher,
                             std::shared_ptr<Bokken::Canvas::Node> node,
                             AssetPack *assets, int layer)
            {
                if (!node || node->textContent.empty())
                    return;
                if (!s_glyphCache)
                    return;

                const auto &s = node->style;
                const float fSize = s.fontSize > 0.f ? s.fontSize : 16.f;
                const std::string fontPath = s.font.empty() ? std::string("fonts/default.ttf") : s.font;

                const uint32_t rgba = s.color;
                const float colorA = ((rgba >> 0) & 0xFFu) / 255.0f;
                const float finalA = std::clamp(colorA * s.opacity, 0.0f, 1.0f);
                const uint8_t alphaB = (uint8_t)(finalA * 255.0f + 0.5f);
                const uint32_t tint = (rgba & 0xFFFFFF00u) | alphaB;

                const float pT = resolveSide(s.paddingTop, s.padding);
                const float pB = resolveSide(s.paddingBottom, s.padding);
                const float pL = resolveSide(s.paddingLeft, s.padding);
                const float pR = resolveSide(s.paddingRight, s.padding);

                TTF_Font *ttf = Label::get_font(fontPath, fSize, assets);
                const float ascent = ttf ? (float)TTF_GetFontAscent(ttf) : fSize * 0.8f;

                const float textW = node->measuredW;
                const float textH = node->measuredH;

                // Horizontal pen origin.
                float penX;
                if (s.width > 0 || s.widthIsPercent)
                {
                    const float contentLeft = node->layout.x + pL;
                    const float contentRight = node->layout.x + node->layout.w - pR;
                    const float contentW = contentRight - contentLeft;

                    if (s.justifyContent == Align::Center)
                        penX = contentLeft + (contentW - textW) * 0.5f;
                    else if (s.justifyContent == Align::End)
                        penX = contentRight - textW;
                    else
                        penX = contentLeft;
                }
                else
                {
                    penX = node->layout.x + pL;
                }

                // Vertical baseline.
                float baseline;
                if (s.height > 0 || s.heightIsPercent)
                {
                    const float contentTop = node->layout.y + pT;
                    const float contentBottom = node->layout.y + node->layout.h - pB;
                    const float contentH = contentBottom - contentTop;

                    float topOfLine;
                    if (s.alignItems == Align::Center)
                        topOfLine = contentTop + (contentH - textH) * 0.5f;
                    else if (s.alignItems == Align::End)
                        topOfLine = contentBottom - textH;
                    else
                        topOfLine = contentTop;
                    baseline = topOfLine + ascent;
                }
                else
                {
                    baseline = node->layout.y + pT + ascent;
                }

                if (ttf)
                {
                    const char *fp = node->textContent.data();
                    const char *fe = fp + node->textContent.size();
                    int vMaxY = 0, vMinY = 0;
                    bool hasInk = false;
                    const char *tp = fp;

                    while (tp < fe)
                    {
                        uint32_t cp = 0;
                        size_t bytes = decodeUTF8(tp, fe, &cp);
                        if (bytes == 0)
                            break;
                        tp += bytes;
                        int gMinX, gMaxX, gMinY, gMaxY, gAdv;
                        if (TTF_GetGlyphMetrics(ttf, cp, &gMinX, &gMaxX, &gMinY, &gMaxY, &gAdv) && gMaxX > gMinX)
                        {
                            if (!hasInk)
                            {
                                vMaxY = gMaxY;
                                vMinY = gMinY;
                                hasInk = true;
                            }
                            else
                            {
                                vMaxY = std::max(vMaxY, gMaxY);
                                vMinY = std::min(vMinY, gMinY);
                            }
                        }
                    }

                    if (hasInk)
                    {
                        float lineCenter = (baseline - ascent) + textH * 0.5f;
                        float inkMidY = ((float)vMaxY + (float)vMinY) * 0.5f;
                        baseline = lineCenter + inkMidY;
                    }
                }

                const float scale = node->getGlobalScale();
                const bool isScaling = (scale != 1.0f);

                if (!isScaling)
                {
                    penX = std::round(penX);
                    baseline = std::round(baseline);
                }

                const float originX = penX + textW * 0.5f;
                const float originY = (baseline - ascent) + textH * 0.5f;

                const Renderer::Texture2D *atlas = s_glyphCache->atlas();
                if (!atlas || !atlas->isValid())
                    return;
                const float atlasW = (float)atlas->width();
                const float atlasH = (float)atlas->height();

                float remainder = 0.0f;

                const char *p = node->textContent.data();
                const char *end = p + node->textContent.size();
                while (p < end)
                {
                    uint32_t cp = 0;
                    size_t adv = decodeUTF8(p, end, &cp);
                    if (adv == 0)
                        break;
                    p += adv;

                    const auto *g = s_glyphCache->getGlyph(fontPath, fSize, assets, cp);
                    if (!g)
                        continue;

                    if (g->width == 0 || g->height == 0)
                    {
                        if (isScaling)
                        {
                            penX += (float)g->advance * scale;
                        }
                        else
                        {
                            float rawAdv = (float)g->advance + remainder;
                            float snapped = std::round(rawAdv);
                            remainder = rawAdv - snapped;
                            penX += snapped;
                        }
                        continue;
                    }

                    // With tightly-cropped bitmaps:
                    //   gx = penX + bearingX   → ink left edge
                    //   gy = baseline - bearingY → ink top edge
                    //   gw/gh = exact ink dimensions
                    float gx = penX + (float)g->bearingX;
                    float gy = baseline - (float)g->bearingY;
                    float gw = (float)g->width;
                    float gh = (float)g->height;

                    if (isScaling)
                    {
                        gx = originX + (gx - originX) * scale;
                        gy = originY + (gy - originY) * scale;
                        gw *= scale;
                        gh *= scale;
                    }
                    else
                    {
                        gx = std::round(gx);
                        gy = std::round(gy);
                        gw = std::round(gw);
                        gh = std::round(gh);
                    }

                    const float u0 = (float)g->u0 / atlasW;
                    const float v0 = (float)g->v0 / atlasH;
                    const float u1 = (float)g->u1 / atlasW;
                    const float v1 = (float)g->v1 / atlasH;

                    batcher.drawTextured(atlas, gx, gy, gw, gh,
                                         u0, v0, u1, v1, tint, layer);

                    if (isScaling)
                    {
                        penX += (float)g->advance * scale;
                    }
                    else
                    {
                        float rawAdv = (float)g->advance + remainder;
                        float snapped = std::round(rawAdv);
                        remainder = rawAdv - snapped;
                        penX += snapped;
                    }
                }
            }

        }
    }
}