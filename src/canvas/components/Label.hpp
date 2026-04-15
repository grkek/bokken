#pragma once
#include "../Node.hpp"
#include "../../AssetPack.hpp"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>
#include <memory>
#include <map>
#include <functional>

namespace Bokken
{
    namespace Canvas
    {
        namespace Components
        {
            class Label
            {
            public:
                Label(const std::string &text) : m_text(text) {}
                void setStyle(const SimpleStyleSheet &s) { m_style = s; }

                std::shared_ptr<Bokken::Canvas::Node> toNode();
                static void computeNode(std::shared_ptr<Bokken::Canvas::Node> node, Bokken::AssetPack *assets);
                static void layoutNode(std::shared_ptr<Bokken::Canvas::Node> node);
                static void draw(SDL_Renderer *r, std::shared_ptr<Bokken::Canvas::Node> n, Bokken::AssetPack *a);
                static TTF_Font *get_font(const std::string &p, float s, Bokken::AssetPack *a, SDL_Renderer *renderer = nullptr);
                static void clear_font_cache();

            private:
                std::string m_text;
                SimpleStyleSheet m_style;
                static inline std::map<std::string, TTF_Font *> s_font_cache;
            };
        }
    }
}