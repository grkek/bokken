#include "View.hpp"
#include <algorithm>

namespace Bokken
{
    namespace Canvas
    {
        namespace Components
        {

            void View::addChild(std::shared_ptr<Bokken::Canvas::Node> child)
            {
                m_children.push_back(std::move(child));
            }

            void View::setStyle(const Bokken::Canvas::SimpleStyleSheet &style)
            {
                m_style = style;
            }

            void View::computeNode(std::shared_ptr<Bokken::Canvas::Node> node, Bokken::AssetPack *assets)
            {
                const auto &s = node->style;
                float pT = (s.paddingTop != 0) ? s.paddingTop : s.padding;
                float pB = (s.paddingBottom != 0) ? s.paddingBottom : s.padding;
                float pL = (s.paddingLeft != 0) ? s.paddingLeft : s.padding;
                float pR = (s.paddingRight != 0) ? s.paddingRight : s.padding;

                float contentW = 0, contentH = 0;

                // First pass: measure all children (they already have their layout.w/h set)
                for (auto &c : node->children)
                {
                    if (c->style.position == Position::Absolute)
                        continue;
                    float mt = (c->style.marginTop != 0) ? c->style.marginTop : c->style.margin;
                    float mb = (c->style.marginBottom != 0) ? c->style.marginBottom : c->style.margin;
                    float ml = (c->style.marginLeft != 0) ? c->style.marginLeft : c->style.margin;
                    float mr = (c->style.marginRight != 0) ? c->style.marginRight : c->style.margin;

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

                float desiredW = contentW + pL + pR;
                float desiredH = contentH + pT + pB;

                float limitW = node->layout.w;
                float limitH = node->layout.h;

                // Clamp to available space if no fixed size
                if (s.width <= 0 && !s.widthIsPercent)
                    node->layout.w = std::min(desiredW, limitW);
                if (s.height <= 0 && !s.heightIsPercent)
                    node->layout.h = std::min(desiredH, limitH);

                // No visual scale clamping here — that's for animations only
                node->visualScale = 1.0f;
            }

            void View::layoutNode(std::shared_ptr<Bokken::Canvas::Node> node)
            {
                const auto &s = node->style;

                // 1. Resolve effective padding
                float pT = (s.paddingTop != 0) ? s.paddingTop : s.padding;
                float pL = (s.paddingLeft != 0) ? s.paddingLeft : s.padding;
                float pR = (s.paddingRight != 0) ? s.paddingRight : s.padding;
                float pB = (s.paddingBottom != 0) ? s.paddingBottom : s.padding;

                // 2. Gather non-absolute children and separate fixed from flexible
                struct ChildInfo
                {
                    std::shared_ptr<Node> node;
                    float mt, mb, ml, mr; // margins
                    float mainSize;       // size along main axis (height for column, width for row)
                    float crossSize;      // size along cross axis
                    float flex;           // flex-grow factor
                };
                std::vector<ChildInfo> children;
                float totalMainFixed = 0.0f; // sum of (mainSize + main margins) for fixed children
                float totalFlex = 0.0f;
                float maxCross = 0.0f; // max cross size (for container sizing if needed)

                for (auto &c : node->children)
                {
                    if (c->style.position == Position::Absolute)
                        continue;

                    ChildInfo info;
                    info.node = c;
                    info.mt = (c->style.marginTop != 0) ? c->style.marginTop : c->style.margin;
                    info.mb = (c->style.marginBottom != 0) ? c->style.marginBottom : c->style.margin;
                    info.ml = (c->style.marginLeft != 0) ? c->style.marginLeft : c->style.margin;
                    info.mr = (c->style.marginRight != 0) ? c->style.marginRight : c->style.margin;
                    info.flex = c->style.flex;

                    if (s.flexDirection == FlexDirection::Column)
                    {
                        info.mainSize = c->layout.h;
                        info.crossSize = c->layout.w;
                        float mainMargin = info.mt + info.mb;
                        if (info.flex > 0)
                            totalFlex += info.flex;
                        else
                            totalMainFixed += info.mainSize + mainMargin;
                        maxCross = std::max(maxCross, info.crossSize + info.ml + info.mr);
                    }
                    else // Row
                    {
                        info.mainSize = c->layout.w;
                        info.crossSize = c->layout.h;
                        float mainMargin = info.ml + info.mr;
                        if (info.flex > 0)
                            totalFlex += info.flex;
                        else
                            totalMainFixed += info.mainSize + mainMargin;
                        maxCross = std::max(maxCross, info.crossSize + info.mt + info.mb);
                    }
                    children.push_back(info);
                }

                // 3. Calculate remaining space along main axis
                float containerMainSize = (s.flexDirection == FlexDirection::Column)
                                              ? node->layout.h - pT - pB
                                              : node->layout.w - pL - pR;
                float remainingMain = containerMainSize - totalMainFixed;

                // 4. Distribute remaining space to flexible children
                if (totalFlex > 0 && remainingMain > 0)
                {
                    for (auto &info : children)
                    {
                        if (info.flex > 0)
                        {
                            float extra = remainingMain * (info.flex / totalFlex);
                            info.mainSize += extra;
                            // Update the actual node's layout size
                            if (s.flexDirection == FlexDirection::Column)
                                info.node->layout.h = info.mainSize;
                            else
                                info.node->layout.w = info.mainSize;
                        }
                    }
                    // Recalculate total main size after flex distribution
                    totalMainFixed = 0.0f;
                    for (auto &info : children)
                    {
                        float mainMargin = (s.flexDirection == FlexDirection::Column)
                                               ? info.mt + info.mb
                                               : info.ml + info.mr;
                        totalMainFixed += info.mainSize + mainMargin;
                    }
                }

                // 5. Determine starting position based on justifyContent
                float startX = node->layout.x + pL;
                float startY = node->layout.y + pT;

                if (s.justifyContent == Align::Center)
                {
                    if (s.flexDirection == FlexDirection::Column)
                        startY += (containerMainSize - totalMainFixed) / 2.0f;
                    else
                        startX += (containerMainSize - totalMainFixed) / 2.0f;
                }
                else if (s.justifyContent == Align::End)
                {
                    if (s.flexDirection == FlexDirection::Column)
                        startY += (containerMainSize - totalMainFixed);
                    else
                        startX += (containerMainSize - totalMainFixed);
                }
                // Align::Start (default) keeps startY/startX unchanged

                float currentX = startX;
                float currentY = startY;

                // 6. Position children
                for (auto &info : children)
                {
                    auto &child = info.node;
                    if (s.flexDirection == FlexDirection::Column)
                    {
                        // Cross-axis (horizontal) alignment
                        float contentLeft = node->layout.x + pL;
                        float contentRight = node->layout.x + node->layout.w - pR;
                        float contentWidth = contentRight - contentLeft;

                        if (s.alignItems == Align::Center)
                            child->layout.x = contentLeft + (contentWidth - child->layout.w) / 2.0f;
                        else if (s.alignItems == Align::End)
                            child->layout.x = contentRight - child->layout.w - info.mr;
                        else // Start
                            child->layout.x = contentLeft + info.ml;

                        // Main-axis (vertical)
                        child->layout.y = currentY + info.mt;
                        currentY += child->layout.h + info.mt + info.mb;
                    }
                    else // Row
                    {
                        // Cross-axis (vertical) alignment
                        float contentTop = node->layout.y + pT;
                        float contentBottom = node->layout.y + node->layout.h - pB;
                        float contentHeight = contentBottom - contentTop;

                        if (s.alignItems == Align::Center)
                            child->layout.y = contentTop + (contentHeight - child->layout.h) / 2.0f;
                        else if (s.alignItems == Align::End)
                            child->layout.y = contentBottom - child->layout.h - info.mb;
                        else // Start
                            child->layout.y = contentTop + info.mt;

                        // Main-axis (horizontal)
                        child->layout.x = currentX + info.ml;
                        currentX += child->layout.w + info.ml + info.mr;
                    }

                    // Recursively layout child
                    if (child->onLayout)
                        child->onLayout(child);
                }

                // 7. Handle absolute positioned children
                for (auto &child : node->children)
                {
                    if (child->style.position == Position::Absolute)
                    {
                        // Resolve left/right/top/bottom
                        float left = child->style.left;
                        float right = child->style.right;
                        float top = child->style.top;
                        float bottom = child->style.bottom;

                        // If both left and right are set, stretch width
                        if (left != 0 && right != 0)
                        {
                            child->layout.x = node->layout.x + left;
                            child->layout.w = node->layout.w - left - right;
                        }
                        else if (left != 0)
                        {
                            child->layout.x = node->layout.x + left;
                        }
                        else if (right != 0)
                        {
                            child->layout.x = node->layout.x + node->layout.w - child->layout.w - right;
                        }

                        // Vertical positioning
                        if (top != 0 && bottom != 0)
                        {
                            child->layout.y = node->layout.y + top;
                            child->layout.h = node->layout.h - top - bottom;
                        }
                        else if (top != 0)
                        {
                            child->layout.y = node->layout.y + top;
                        }
                        else if (bottom != 0)
                        {
                            child->layout.y = node->layout.y + node->layout.h - child->layout.h - bottom;
                        }

                        if (child->onLayout)
                            child->onLayout(child);
                    }
                }
            }

            std::shared_ptr<Bokken::Canvas::Node> View::toNode()
            {
                auto node = std::make_shared<Bokken::Canvas::Node>("View");

                node->children = m_children;
                node->style = m_style;

                node->onCompute = &computeNode;
                node->onLayout = &layoutNode;

                return node;
            }

            void View::draw(SDL_Renderer *renderer, std::shared_ptr<Node> node)
            {
                if (!renderer || !node)
                    return;

                SDL_FRect rect = {node->layout.x, node->layout.y, node->layout.w, node->layout.h};

                // Visual scaling around center – DO NOT MODIFY node->layout!
                if (node->visualScale != 1.0f)
                {
                    float cx = rect.x + rect.w * 0.5f;
                    float cy = rect.y + rect.h * 0.5f;
                    rect.w *= node->visualScale;
                    rect.h *= node->visualScale;
                    rect.x = cx - rect.w * 0.5f;
                    rect.y = cy - rect.h * 0.5f;
                }

                // Extract colors once
                uint32_t bgColor = node->style.backgroundColor;
                Uint8 r = (bgColor >> 24) & 0xFF;
                Uint8 g = (bgColor >> 16) & 0xFF;
                Uint8 b = (bgColor >> 8) & 0xFF;
                Uint8 a = (bgColor >> 0) & 0xFF;

                uint32_t borderColor = node->style.borderColor;
                Uint8 br = (borderColor >> 24) & 0xFF;
                Uint8 bgC = (borderColor >> 16) & 0xFF; // renamed to avoid conflict with background 'g'
                Uint8 bb = (borderColor >> 8) & 0xFF;
                Uint8 ba = (borderColor >> 0) & 0xFF;

                float bw = node->style.borderWidth;

                // Decide draw order based on border presence
                bool hasBorder = (bw > 0.0f && ba > 0);
                bool hasBackground = (a > 0);

                if (hasBorder)
                {
                    // Draw the border as a filled rectangle (outermost)
                    SDL_SetRenderDrawColor(renderer, br, bgC, bb, ba);
                    SDL_RenderFillRect(renderer, &rect);

                    // Draw the background inset by border width
                    if (hasBackground)
                    {
                        SDL_FRect innerRect = {
                            rect.x + bw,
                            rect.y + bw,
                            rect.w - 2 * bw,
                            rect.h - 2 * bw};
                        SDL_SetRenderDrawColor(renderer, r, g, b, a);
                        SDL_RenderFillRect(renderer, &innerRect);
                    }
                }
                else if (hasBackground)
                {
                    // No border, just background
                    SDL_SetRenderDrawColor(renderer, r, g, b, a);
                    SDL_RenderFillRect(renderer, &rect);
                }
            }

        } // namespace Components
    } // namespace Canvas
} // namespace Bokken