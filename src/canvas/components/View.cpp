#include "View.hpp"
#include "../../renderer/SpriteBatcher.hpp"
#include <algorithm>
#include <cmath>

namespace Bokken
{
    namespace Canvas
    {
        namespace Components
        {

            void View::addChild(std::shared_ptr<Node> child)
            {
                m_children.push_back(std::move(child));
            }

            void View::setStyle(const SimpleStyleSheet &style)
            {
                m_style = style;
            }

            void View::computeNode(std::shared_ptr<Node> node, AssetPack *assets)
            {
                const auto &s = node->style;
                // resolveSide() returns the per-side override if set (non-NaN),
                // otherwise falls back to the shorthand. The previous `!= 0`
                // test couldn't distinguish "user set 0" from "user didn't set
                // it", which broke layouts that intentionally zeroed one side.
                const float pT = resolveSide(s.paddingTop,    s.padding);
                const float pB = resolveSide(s.paddingBottom, s.padding);
                const float pL = resolveSide(s.paddingLeft,   s.padding);
                const float pR = resolveSide(s.paddingRight,  s.padding);

                float contentW = 0, contentH = 0;

                for (auto &c : node->children)
                {
                    if (c->style.position == Position::Absolute)
                        continue;
                    const float mt = resolveSide(c->style.marginTop,    c->style.margin);
                    const float mb = resolveSide(c->style.marginBottom, c->style.margin);
                    const float ml = resolveSide(c->style.marginLeft,   c->style.margin);
                    const float mr = resolveSide(c->style.marginRight,  c->style.margin);

                    if (s.flexDirection == FlexDirection::Column)
                    {
                        contentH += c->layout.h + mt + mb;
                        contentW = std::max(contentW, c->layout.w + ml + mr);
                    }
                    else
                    {
                        contentW += c->layout.w + ml + mr;
                        contentH = std::max(contentH, c->layout.h + mt + mb);
                    }
                }

                const float desiredW = contentW + pL + pR;
                const float desiredH = contentH + pT + pB;

                const float limitW = node->layout.w;
                const float limitH = node->layout.h;

                if (s.width <= 0 && !s.widthIsPercent)
                    node->layout.w = std::min(desiredW, limitW);
                if (s.height <= 0 && !s.heightIsPercent)
                    node->layout.h = std::min(desiredH, limitH);

                node->visualScale = 1.0f;
            }

            void View::layoutNode(std::shared_ptr<Node> node)
            {
                const auto &s = node->style;

                const float pT = resolveSide(s.paddingTop,    s.padding);
                const float pL = resolveSide(s.paddingLeft,   s.padding);
                const float pR = resolveSide(s.paddingRight,  s.padding);
                const float pB = resolveSide(s.paddingBottom, s.padding);

                struct ChildInfo
                {
                    std::shared_ptr<Node> node;
                    float mt, mb, ml, mr;
                    float mainSize;
                    float crossSize;
                    float flex;
                };
                std::vector<ChildInfo> children;
                float totalMainFixed = 0.0f;
                float totalFlex = 0.0f;
                float maxCross = 0.0f;

                for (auto &c : node->children)
                {
                    if (c->style.position == Position::Absolute) continue;

                    ChildInfo info;
                    info.node = c;
                    info.mt   = resolveSide(c->style.marginTop,    c->style.margin);
                    info.mb   = resolveSide(c->style.marginBottom, c->style.margin);
                    info.ml   = resolveSide(c->style.marginLeft,   c->style.margin);
                    info.mr   = resolveSide(c->style.marginRight,  c->style.margin);
                    info.flex = c->style.flex;

                    if (s.flexDirection == FlexDirection::Column)
                    {
                        info.mainSize = c->layout.h;
                        info.crossSize = c->layout.w;
                        const float mainMargin = info.mt + info.mb;
                        if (info.flex > 0) totalFlex += info.flex;
                        else               totalMainFixed += info.mainSize + mainMargin;
                        maxCross = std::max(maxCross, info.crossSize + info.ml + info.mr);
                    }
                    else
                    {
                        info.mainSize = c->layout.w;
                        info.crossSize = c->layout.h;
                        const float mainMargin = info.ml + info.mr;
                        if (info.flex > 0) totalFlex += info.flex;
                        else               totalMainFixed += info.mainSize + mainMargin;
                        maxCross = std::max(maxCross, info.crossSize + info.mt + info.mb);
                    }
                    children.push_back(info);
                }

                const float containerMainSize = (s.flexDirection == FlexDirection::Column)
                    ? node->layout.h - pT - pB : node->layout.w - pL - pR;
                const float remainingMain = containerMainSize - totalMainFixed;

                if (totalFlex > 0 && remainingMain > 0)
                {
                    for (auto &info : children)
                    {
                        if (info.flex > 0)
                        {
                            const float extra = remainingMain * (info.flex / totalFlex);
                            info.mainSize += extra;
                            if (s.flexDirection == FlexDirection::Column)
                                info.node->layout.h = info.mainSize;
                            else
                                info.node->layout.w = info.mainSize;
                        }
                    }
                    totalMainFixed = 0.0f;
                    for (auto &info : children)
                    {
                        const float mainMargin = (s.flexDirection == FlexDirection::Column)
                            ? info.mt + info.mb : info.ml + info.mr;
                        totalMainFixed += info.mainSize + mainMargin;
                    }
                }

                float startX = node->layout.x + pL;
                float startY = node->layout.y + pT;

                if (s.justifyContent == Align::Center)
                {
                    if (s.flexDirection == FlexDirection::Column) startY += (containerMainSize - totalMainFixed) / 2.0f;
                    else                                          startX += (containerMainSize - totalMainFixed) / 2.0f;
                }
                else if (s.justifyContent == Align::End)
                {
                    if (s.flexDirection == FlexDirection::Column) startY += (containerMainSize - totalMainFixed);
                    else                                          startX += (containerMainSize - totalMainFixed);
                }

                float currentX = startX, currentY = startY;

                for (auto &info : children)
                {
                    auto &child = info.node;
                    if (s.flexDirection == FlexDirection::Column)
                    {
                        const float contentLeft  = node->layout.x + pL;
                        const float contentRight = node->layout.x + node->layout.w - pR;
                        const float contentWidth = contentRight - contentLeft;

                        // Pixel-snap cross-axis position to avoid fractional
                        // offsets that cascade into child text rendering.
                        if (s.alignItems == Align::Center)
                            child->layout.x = std::round(contentLeft + (contentWidth - child->layout.w) / 2.0f);
                        else if (s.alignItems == Align::End)
                            child->layout.x = std::round(contentRight - child->layout.w - info.mr);
                        else
                            child->layout.x = std::round(contentLeft + info.ml);

                        child->layout.y = std::round(currentY + info.mt);
                        currentY += child->layout.h + info.mt + info.mb;
                    }
                    else
                    {
                        const float contentTop    = node->layout.y + pT;
                        const float contentBottom = node->layout.y + node->layout.h - pB;
                        const float contentHeight = contentBottom - contentTop;

                        if (s.alignItems == Align::Center)
                            child->layout.y = std::round(contentTop + (contentHeight - child->layout.h) / 2.0f);
                        else if (s.alignItems == Align::End)
                            child->layout.y = std::round(contentBottom - child->layout.h - info.mb);
                        else
                            child->layout.y = std::round(contentTop + info.mt);

                        child->layout.x = std::round(currentX + info.ml);
                        currentX += child->layout.w + info.ml + info.mr;
                    }

                    if (child->onLayout) child->onLayout(child);
                }

                // ── Absolute-positioned children ──
                //
                // top/bottom/left/right default to NaN ("not pinned"). We
                // probe each via isSet(); the previous `!= 0` test made it
                // impossible to pin a child to top:0 / left:0 / etc.
                //
                // Positions are pixel-snapped to prevent fractional offsets
                // from cascading into text rendering.
                for (auto &child : node->children)
                {
                    if (child->style.position != Position::Absolute) continue;

                    const float left   = child->style.left;
                    const float right  = child->style.right;
                    const float top    = child->style.top;
                    const float bottom = child->style.bottom;

                    if (isSet(left) && isSet(right))
                    {
                        child->layout.x = std::round(node->layout.x + left);
                        child->layout.w = std::round(node->layout.w - left - right);
                    }
                    else if (isSet(left))
                    {
                        child->layout.x = std::round(node->layout.x + left);
                    }
                    else if (isSet(right))
                    {
                        child->layout.x = std::round(node->layout.x + node->layout.w - child->layout.w - right);
                    }

                    if (isSet(top) && isSet(bottom))
                    {
                        child->layout.y = std::round(node->layout.y + top);
                        child->layout.h = std::round(node->layout.h - top - bottom);
                    }
                    else if (isSet(top))
                    {
                        child->layout.y = std::round(node->layout.y + top);
                    }
                    else if (isSet(bottom))
                    {
                        child->layout.y = std::round(node->layout.y + node->layout.h - child->layout.h - bottom);
                    }

                    if (child->onLayout) child->onLayout(child);
                }
            }

            std::shared_ptr<Node> View::toNode()
            {
                auto node = std::make_shared<Node>("View");
                node->children = m_children;
                node->style = m_style;
                node->onCompute = &computeNode;
                node->onLayout = &layoutNode;
                return node;
            }

            void View::draw(Renderer::SpriteBatcher &batcher,
                             std::shared_ptr<Node> node, int layer)
            {
                if (!node) return;

                // Apply visual scale (hover/active animations) without
                // mutating layout — same trick as before.
                float x = node->layout.x;
                float y = node->layout.y;
                float w = node->layout.w;
                float h = node->layout.h;
                if (node->visualScale != 1.0f)
                {
                    const float cx = x + w * 0.5f;
                    const float cy = y + h * 0.5f;
                    w *= node->visualScale;
                    h *= node->visualScale;
                    x = cx - w * 0.5f;
                    y = cy - h * 0.5f;
                }

                const uint32_t bg = node->style.backgroundColor;
                const uint32_t bd = node->style.borderColor;
                const float    bw = node->style.borderWidth;
                const bool hasBorder = (bw > 0.0f && (bd & 0xFF) > 0);
                const bool hasBg     = ((bg & 0xFF) > 0);

                // ── Border drawing ──
                //
                // The previous version drew the full bounding rect in border
                // colour and then layered the bg rect on top, relying on the
                // 1-px (or `bw`-px) ring being visible. That works ONLY if
                // there's a background — `borderWidth: 2` with a transparent
                // background produced a fully-filled rectangle in the border
                // colour, which was clearly wrong.
                //
                // We now draw the border as four edge rects so the inside
                // is left untouched. Then the background, if any, layers
                // inside the inset.
                if (hasBorder)
                {
                    // Top, bottom, left, right edges. Corners belong to the
                    // top/bottom edges so the verticals are inset by `bw`.
                    batcher.drawRect(x,           y,           w,            bw,           bd, layer);
                    batcher.drawRect(x,           y + h - bw,  w,            bw,           bd, layer);
                    batcher.drawRect(x,           y + bw,      bw,           h - 2.0f * bw, bd, layer);
                    batcher.drawRect(x + w - bw,  y + bw,      bw,           h - 2.0f * bw, bd, layer);

                    if (hasBg)
                        batcher.drawRect(x + bw, y + bw, w - 2.0f * bw, h - 2.0f * bw, bg, layer + 1);
                }
                else if (hasBg)
                {
                    batcher.drawRect(x, y, w, h, bg, layer);
                }
            }

        }
    }
}
