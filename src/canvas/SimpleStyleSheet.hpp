#pragma once

#include "Align.hpp"
#include "Timing.hpp"
#include "FlexDirection.hpp"
#include "Position.hpp"
#include "Overflow.hpp"
#include <string>
#include <cstdint>

namespace Bokken
{
    namespace Canvas
    {
        /**
         * Native representation of the Simple Style Sheet (SSS).
         * Used by the Layout Engine to calculate Node geometry and rendering state.
         */
        struct SimpleStyleSheet
        {
            /** Layout & Flexbox **/

            /* Direction of child flow (Default: Column) */
            Bokken::Canvas::FlexDirection flexDirection = Bokken::Canvas::FlexDirection::Column;

            /* Growth factor relative to siblings */
            float flex = 0.0f;

            /* Base internal spacing */
            float padding = 0.0f;

            /* Granular internal spacing */
            float paddingTop = 0.0f;
            float paddingBottom = 0.0f;
            float paddingLeft = 0.0f;
            float paddingRight = 0.0f;

            /* Base external spacing */
            float margin = 0.0f;

            /* Granular external spacing (The fix for your button layout) */
            float marginTop = 0.0f;
            float marginBottom = 0.0f;
            float marginLeft = 0.0f;
            float marginRight = 0.0f;

            /** Positioning **/

            /* Positioning strategy (Default: Relative) */
            Bokken::Canvas::Position position = Bokken::Canvas::Position::Relative;

            /* Coordinates for absolute positioning */
            float top = 0.0f;
            float bottom = 0.0f;
            float left = 0.0f;
            float right = 0.0f;

            /* Rendering order priority */
            int32_t zIndex = 0;

            /** Sizing & Fonts **/

            std::string font = "fonts/default.ttf";
            float fontSize = 16.0f;

            float width = 0.0f;
            bool widthIsPercent = false;
            float height = 0.0f;
            bool heightIsPercent = false;

            /** Visual Styling **/

            /* Background color in 0xRRGGBBAA format */
            uint32_t backgroundColor = 0x00000000;

            /* Text/Foreground color in 0xRRGGBBAA format */
            uint32_t color = 0xFFFFFFFF;

            /* Transparency level from 0.0f to 1.0f */
            float opacity = 1.0f;

            /* Corner rounding radius in pixels */
            float borderRadius = 0.0f;

            /* Border thickness and color */
            float borderWidth = 0.0f;
            uint32_t borderColor = 0x00000000;

            /* Clipping behavior for child elements */
            Bokken::Canvas::Overflow overflow = Bokken::Canvas::Overflow::Visible;

            /** Alignment **/

            /* Distribution along the main axis */
            Bokken::Canvas::Align justifyContent = Bokken::Canvas::Align::Start;

            /* Alignment along the cross axis */
            Bokken::Canvas::Align alignItems = Bokken::Canvas::Align::Center;

            /** Animation properties **/

            /* Animation duration in seconds */
            float transitionDuration = 0.0f;

            /* The curve used for transitions */
            Bokken::Canvas::Timing transitionTiming = Bokken::Canvas::Timing::Linear;

            /** Target states for interaction animations **/

            /* Multiplier when mouse is hovering */
            float hoverScale = 1.0f;

            /* Multiplier when active/clicked */
            float activeScale = 0.95f;
        };
    }
}