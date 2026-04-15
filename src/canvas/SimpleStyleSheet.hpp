#pragma once

#include "Align.hpp"
#include <string>

namespace Bokken
{
    namespace Canvas
    {
        struct SimpleStyleSheet
        {
            std::string flexDirection = "column";
            std::string font = "fonts/default.ttf";
            float width = 0;
            bool widthIsPercent = false;
            float height = 0;
            bool heightIsPercent = false;
            float padding = 0.0f, margin = 0.0f;
            Bokken::Canvas::Align justifyContent = Bokken::Canvas::Align::Start;
            Bokken::Canvas::Align alignItems = Bokken::Canvas::Align::Center;
            uint32_t backgroundColor = 0x00000000;
            uint32_t color = 0xFFFFFFFF;
            float fontSize = 16.0f;
            float opacity = 1.0f;
            float borderRadius = 0.0f;
        };
    }
}