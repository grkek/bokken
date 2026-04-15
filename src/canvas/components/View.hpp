#pragma once

#include "../../canvas/Node.hpp"
#include "../SimpleStyleSheet.hpp"
#include <SDL3/SDL.h>
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
                void addChild(std::shared_ptr<Bokken::Canvas::Node> child);
                void setStyle(const Bokken::Canvas::SimpleStyleSheet &style);
                std::shared_ptr<Bokken::Canvas::Node> toNode();

                // Static drawing function which draws background rect, then recurses into children
                static void draw(SDL_Renderer *renderer, std::shared_ptr<Bokken::Canvas::Node> node);

            private:
                std::vector<std::shared_ptr<Bokken::Canvas::Node>> m_children;
                Bokken::Canvas::SimpleStyleSheet m_style;
            };
        }
    }
}