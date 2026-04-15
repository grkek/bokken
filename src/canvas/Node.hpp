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

namespace Bokken
{

    namespace Canvas
    {
        class Node : public std::enable_shared_from_this<Node>
        {
        public:
            Node *parent = nullptr;

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
            bool isHovered = false;
            bool isActive = false;

            float visualScale = 1.0f; /* The actual scale used for drawing */
            float startScale = 1.0f;  /* The scale at the start of a transition */
            float targetScale = 1.0f; /* The goal scale */
            float animationTimer = 0.0f;

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

            void add_child(std::shared_ptr<Node> child)
            {
                child->parent = this;
                children.push_back(child);
            }

            float getGlobalScale() const
            {
                float scale = visualScale;
                Node *p = parent;
                while (p)
                {
                    scale *= p->visualScale;
                    p = p->parent;
                }
                return scale;
            }

            void compute(float startX, float startY, float maxWidth, float maxHeight, AssetPack *assets)
            {

                if (style.widthIsPercent)
                    layout.w = maxWidth * (style.width / 100.0f);
                else if (style.width > 0)
                    layout.w = style.width;
                else
                    layout.w = maxWidth;

                if (style.heightIsPercent)
                    layout.h = maxHeight * (style.height / 100.0f);
                else if (style.height > 0)
                    layout.h = style.height;
                else
                    layout.h = maxHeight;

                for (auto &child : children)
                {
                    child->compute(0, 0, layout.w, layout.h, assets);
                }

                if (onCompute)
                    onCompute(shared_from_this(), assets);

                layout.x = startX;
                layout.y = startY;
            }
        };
    }
}