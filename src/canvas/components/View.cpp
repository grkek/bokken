#include "View.hpp"

namespace Bokken::Canvas::Components
{
    void View::addChild(std::shared_ptr<Bokken::Canvas::Node> child)
    {
        m_children.push_back(std::move(child));
    }

    void View::setStyle(const Bokken::Canvas::SimpleStyleSheet &style)
    {
        m_style = style;
    }

    std::shared_ptr<Bokken::Canvas::Node> View::toNode()
    {
        auto node = std::make_shared<Bokken::Canvas::Node>("View");
        node->children = m_children;
        node->style = m_style;

        // Measurement
        node->onCompute = [](std::shared_ptr<Bokken::Canvas::Node> n, Bokken::AssetPack *a)
        {
            // 1. Resolve this Node's dimensions first
            // If it's the root, layout.w was set in present() via SDL_GetRenderOutputSize.
            // If it's a child, we assume a fixed size or content-fit for now.
            if (n->style.widthIsPercent)
            {
                n->layout.w = n->layout.w * (n->style.width / 100.0f);
            }
            else if (n->style.width > 0)
            {
                n->layout.w = n->style.width;
            }

            if (n->style.heightIsPercent)
            {
                n->layout.h = n->layout.h * (n->style.height / 100.0f);
            }
            else if (n->style.height > 0)
            {
                n->layout.h = n->style.height;
            }

            // 2. Measure Children (Bottom-Up)
            float totalH = 0;
            float maxW = 0;
            for (auto &child : n->children)
            {
                // Pass parent dimensions down so child percentages can resolve
                child->layout.w = n->layout.w;
                child->layout.h = n->layout.h;

                if (child->onCompute)
                    child->onCompute(child, a);

                maxW = std::max(maxW, child->layout.w + (child->style.margin * 2));
                totalH += child->layout.h + (child->style.margin * 2);
            }

            // 3. Shrink-wrap if no size was specified
            if (n->layout.w <= 0)
                n->layout.w = maxW;
            if (n->layout.h <= 0)
                n->layout.h = totalH;
        };

        // Layout
        node->onLayout = [](std::shared_ptr<Bokken::Canvas::Node> n)
        {
            float currentY = n->layout.y;

            // Calculate vertical start for centering
            float contentH = 0;
            for (auto &c : n->children)
                contentH += c->layout.h + (c->style.margin * 2);

            if (n->style.justifyContent == Bokken::Canvas::Align::Center)
            {
                currentY += (n->layout.h - contentH) / 2.0f;
            }

            for (auto &child : n->children)
            {
                currentY += child->style.margin;

                // Set child X
                if (n->style.alignItems == Bokken::Canvas::Align::Center)
                {
                    child->layout.x = n->layout.x + (n->layout.w - child->layout.w) / 2.0f;
                }
                else
                {
                    child->layout.x = n->layout.x + child->style.margin;
                }

                // Set child Y
                child->layout.y = currentY;
                currentY += child->layout.h + child->style.margin;

                // Now that child has its global X/Y, tell it to position its own children
                if (child->onLayout)
                    child->onLayout(child);
            }
        };

        return node;
    }
    void View::draw(SDL_Renderer *renderer, std::shared_ptr<Node> node)
    {
        if (!renderer || !node)
            return;

        // Apply visualScale (the "squish" animation) to the rect
        SDL_FRect rect = {node->layout.x, node->layout.y, node->layout.w, node->layout.h};

        if (node->visualScale != 1.0f)
        {
            float cx = rect.x + rect.w * 0.5f;
            float cy = rect.y + rect.h * 0.5f;
            rect.w *= node->visualScale;
            rect.h *= node->visualScale;
            rect.x = cx - rect.w * 0.5f;
            rect.y = cy - rect.h * 0.5f;
        }

        // Draw background
        if ((node->style.backgroundColor & 0xFF) != 0) // Check Alpha
        {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

            // Extract RGBA from your 0xRRGGBBAA format
            Uint8 r = (node->style.backgroundColor >> 24) & 0xFF;
            Uint8 g = (node->style.backgroundColor >> 16) & 0xFF;
            Uint8 b = (node->style.backgroundColor >> 8) & 0xFF;
            Uint8 a = (node->style.backgroundColor) & 0xFF;

            SDL_SetRenderDrawColor(renderer, r, g, b, a);
            SDL_RenderFillRect(renderer, &rect);
        }
    }
}