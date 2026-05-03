#pragma once

#include "../../canvas/Node.hpp"
#include "../SimpleStyleSheet.hpp"
#include <SDL3/SDL.h>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

namespace Bokken
{
    namespace Canvas
    {
        namespace Components
        {

            class View
            {
            public:
                View() = default;
                void addChild(std::shared_ptr<Node> child);
                void setStyle(const SimpleStyleSheet &style);
                static void computeNode(std::shared_ptr<Node> node, AssetPack *assets);
                static void layoutNode(std::shared_ptr<Node> node);
                std::shared_ptr<Node> toNode();

                // Static drawing function which draws background rect, then recurses into children
                static void draw(SDL_Renderer *renderer, std::shared_ptr<Node> node);

            private:
                std::vector<std::shared_ptr<Node>> m_children;
                SimpleStyleSheet m_style;
            };
        }
    }
}