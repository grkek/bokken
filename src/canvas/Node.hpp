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
#include <algorithm>

namespace Bokken
{

    namespace Canvas
    {
        /**
         * Canvas tree node.
         *
         * After the GL refactor the Node no longer owns an SDL_Texture —
         * text rendering goes through the engine-wide GlyphCache + batcher,
         * so per-Label textures are gone. The `needsRepaint` flag survives
         * because measurement still benefits from caching; it just no
         * longer guards a texture upload.
         */
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

            /** Layout invalidation flag — set when text or style changes. */
            bool needsRepaint = true;

            /** Cached intrinsic size from the last measure pass —
             *  populated by Label::compute, consumed by parent layout. */
            float measuredW = 0.0f;
            float measuredH = 0.0f;

            /** The calculated screen-space coordinates and dimensions **/
            Rect layout{0, 0, 0, 0};

            /** Animation state **/
            bool isHovered = false;
            bool isActive = false;

            float visualScale = 1.0f;
            float startScale = 1.0f;
            float targetScale = 1.0f;
            float animationTimer = 0.0f;

            /** Measurement (bottom-up) — sets measured size from content. */
            std::function<void(std::shared_ptr<Node>, AssetPack *)> onCompute = nullptr;

            /** Placement (top-down) — set absolute layout from parent. */
            std::function<void(std::shared_ptr<Node>)> onLayout = nullptr;

            std::function<void()> onClick = nullptr;
            std::function<void()> onMouseEnter = nullptr;
            std::function<void()> onMouseLeave = nullptr;
            std::function<void()> onDeconstruct = nullptr;

            explicit Node(std::string t) : type(std::move(t)) {}

            virtual ~Node()
            {
                if (onDeconstruct)
                    onDeconstruct();
            }

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

            /**
             * Two-phase measurement.
             *
             * Phase 1 — resolve THIS node's constraints from the parent.
             * Phase 2 — recurse into children so they measure against the
             *           resolved constraints of this node.
             * Phase 3 — run onCompute (e.g. View::computeNode) which may
             *           shrink-wrap this node based on children's results.
             *
             * The previous version ran children BEFORE onCompute but used
             * the *pre-shrink* layout.w/h as the children's maxWidth/maxHeight.
             * That was mostly fine because View::computeNode only SHRINKS.
             * However, the padding was not subtracted from the available
             * space passed to children, so a child with width:"100%" inside
             * a padded View would overshoot.
             *
             * Fix: pass the content area (after padding) as the children's
             * available space.
             */
            void compute(float startX, float startY, float maxWidth, float maxHeight, AssetPack *assets)
            {
                // 1. Resolve this node's own size from parent constraints.
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

                // 2. Compute the content area available to children
                //    (subtract padding so percentage-sized children and
                //    auto-sized children see the correct available space).
                const float pT = resolveSide(style.paddingTop,    style.padding);
                const float pB = resolveSide(style.paddingBottom,  style.padding);
                const float pL = resolveSide(style.paddingLeft,    style.padding);
                const float pR = resolveSide(style.paddingRight,   style.padding);

                const float childMaxW = std::max(0.0f, layout.w - pL - pR);
                const float childMaxH = std::max(0.0f, layout.h - pT - pB);

                // 3. Recurse: each child measures against the content area.
                for (auto &child : children)
                {
                    child->compute(0, 0, childMaxW, childMaxH, assets);
                }

                // 4. Run the component's own measurement callback.
                //    For View this shrink-wraps around children;
                //    for Label it measures text.
                if (onCompute)
                    onCompute(shared_from_this(), assets);

                layout.x = startX;
                layout.y = startY;
            }
        };
    }
}
