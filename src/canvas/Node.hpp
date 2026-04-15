#pragma once

#include "SimpleStyleSheet.hpp"
#include "Rect.hpp"
#include "Align.hpp"
#include "../AssetPack.hpp"
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <cmath>

namespace Bokken::Canvas
{
    class Node
    {
    public:
        /** The tag or component name (e.g., "View", "Label", "Button") **/
        std::string type;

        /** The text content to be rendered within this node **/
        std::string textContent;

        /** Smart pointers to child nodes for recursive layout and rendering **/
        std::vector<std::shared_ptr<Node>> children;

        /** The styling properties defined via CSS-like shorthand **/
        SimpleStyleSheet style;

        /** Cached GPU texture; typically used for pre-rendered text or images **/
        SDL_Texture *texture = nullptr;

        /** Flag to indicate if the node needs its texture or layout recalculated **/
        bool needsRepaint = true;

        /** The calculated screen-space coordinates and dimensions **/
        Rect layout{0, 0, 0, 0};

        /** Animation state **/
        float visualScale = 1.0f;
        float targetScale = 1.0f;

        /** * The Measurement Phase (Bottom-Up).
         * Executed to determine the intrinsic size of the node based on its
         * content (e.g., measuring SDL_ttf string dimensions or child bounds).
         **/
        std::function<void(std::shared_ptr<Node>, Bokken::AssetPack *)> onCompute = nullptr;

        /** * The Placement Phase (Top-Down).
         * Executed to calculate the absolute screen coordinates (x, y) of the node
         * and its children based on parent bounds and alignment rules.
         **/
        std::function<void(std::shared_ptr<Node>)> onLayout = nullptr;

        std::function<void()> onClick = nullptr;
        std::function<void()> onMouseEnter = nullptr;
        std::function<void()> onMouseLeave = nullptr;
        std::function<void()> onDeconstruct = nullptr;

        explicit Node(std::string t) : type(std::move(t)) {}

        /** Destructor handles explicit cleanup of SDL GPU resources **/
        virtual ~Node()
        {
            if (texture)
            {
                SDL_DestroyTexture(texture);
                texture = nullptr;
            }

            if (onDeconstruct)
                onDeconstruct();
        }

        /** Prevent copying to avoid double-freeing the SDL_Texture pointer **/
        Node(const Node &) = delete;
        Node &operator=(const Node &) = delete;

        void compute(float startX, float startY, float maxWidth, float maxHeight, AssetPack *assets)
        {
            // 1. Establish the Parent's boundaries including margin
            layout.x = startX + style.margin;
            layout.y = startY + style.margin;

            // If explicit width/height isn't set, use remaining available space
            layout.w = (style.width > 0) ? style.width : (maxWidth - (style.margin * 2));
            layout.h = (style.height > 0) ? style.height : (maxHeight - (style.margin * 2));

            // Sanity Check: Prevent NaN propagation from uninitialized styles
            if (std::isnan(layout.x) || std::isnan(layout.y))
            {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "[Compute Error] NaN in %s: x=%.2f, y=%.2f. Margin: %.2f",
                             type.c_str(), layout.x, layout.y, style.margin);
            }

            float currentYOffset = 0;

            for (auto &child : children)
            {
                // Recursive step: compute child layout
                child->compute(layout.x, layout.y + currentYOffset, layout.w, layout.h, assets);

                // Allow child to adjust its own layout based on content (e.g. measuring text)
                if (child->onCompute)
                {
                    child->onCompute(child, assets);
                }

                // Horizontal Alignment (Flexbox-lite)
                if (style.alignItems == Bokken::Canvas::Align::Center)
                {
                    if (child->layout.w > 0 && !std::isnan(child->layout.w))
                    {
                        float spareX = layout.w - child->layout.w;
                        child->layout.x = layout.x + (spareX / 2.0f);
                    }
                }

                // Accumulate vertical offset for the next child
                if (!std::isnan(child->layout.h))
                {
                    currentYOffset += child->layout.h;
                }
            }

            // 2. Vertical Centering (JustifyContent: Center)
            if (style.justifyContent == Bokken::Canvas::Align::Center)
            {
                float spareY = layout.h - currentYOffset;
                float topAdjustment = spareY / 2.0f;

                for (auto &child : children)
                {
                    child->layout.y += topAdjustment;
                }
            }
        }
    };
}