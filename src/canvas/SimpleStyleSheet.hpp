#pragma once

#include "Align.hpp"
#include "Timing.hpp"
#include "FlexDirection.hpp"
#include "Position.hpp"
#include "Overflow.hpp"
#include <string>
#include <cstdint>
#include <cmath>
#include <limits>

namespace Bokken
{
    namespace Canvas
    {
        /**
         * Native representation of the Simple Style Sheet (SSS).
         * Used by the Layout Engine to calculate Node geometry and rendering state.
         *
         * Per-side override convention
         * ---------------------------
         * `padding` and `margin` are the base shorthand values. The per-side
         * fields (paddingTop, marginLeft, ...) override the base ONLY when the
         * user explicitly set them. We encode "user did not set this" as NaN.
         *
         * The previous convention used 0 to mean "unset" — but 0 is also a
         * valid user input ("I want zero top padding even though `padding` is
         * 8"), and the old code couldn't tell the two cases apart. NaN never
         * round-trips through JSON or arithmetic accidentally, so it's a
         * safe sentinel.
         *
         * Use `resolveSide(side, base)` to get the effective value: it returns
         * `side` if set (non-NaN), else `base`.
         *
         * The same scheme applies to `top/bottom/left/right` for absolute
         * positioning — defaulting them to NaN distinguishes "not pinned to
         * that edge" from "pinned at offset 0", which is a meaningful
         * difference (see View::layoutNode).
         */
        struct SimpleStyleSheet
        {
            /** Layout & Flexbox **/

            /* Direction of child flow (Default: Column) */
            FlexDirection flexDirection = FlexDirection::Column;

            /* Growth factor relative to siblings */
            float flex = 0.0f;

            /* Base internal spacing */
            float padding = 0.0f;

            /* Granular internal spacing — NaN means "use `padding`" */
            float paddingTop    = std::numeric_limits<float>::quiet_NaN();
            float paddingBottom = std::numeric_limits<float>::quiet_NaN();
            float paddingLeft   = std::numeric_limits<float>::quiet_NaN();
            float paddingRight  = std::numeric_limits<float>::quiet_NaN();

            /* Base external spacing */
            float margin = 0.0f;

            /* Granular external spacing — NaN means "use `margin`" */
            float marginTop    = std::numeric_limits<float>::quiet_NaN();
            float marginBottom = std::numeric_limits<float>::quiet_NaN();
            float marginLeft   = std::numeric_limits<float>::quiet_NaN();
            float marginRight  = std::numeric_limits<float>::quiet_NaN();

            /** Positioning **/

            /* Positioning strategy (Default: Relative) */
            Bokken::Canvas::Position position = Bokken::Canvas::Position::Relative;

            /* Coordinates for absolute positioning. NaN means "not pinned to
             * that edge" — distinct from 0, which means "pinned at offset 0". */
            float top    = std::numeric_limits<float>::quiet_NaN();
            float bottom = std::numeric_limits<float>::quiet_NaN();
            float left   = std::numeric_limits<float>::quiet_NaN();
            float right  = std::numeric_limits<float>::quiet_NaN();

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

        /**
         * Resolve a per-side override against the shorthand base.
         * Returns `side` if the user set it (non-NaN), else `base`.
         *
         * This is the single source of truth for the
         *   "padding-top falls back to padding"
         * rule. Always call it instead of checking != 0 by hand.
         */
        inline float resolveSide(float side, float base)
        {
            return std::isnan(side) ? base : side;
        }

        /**
         * Test whether a NaN-sentinel field was set by the user.
         * Used for absolute positioning, where the difference between
         * "set to 0" and "not pinned" matters.
         */
        inline bool isSet(float v)
        {
            return !std::isnan(v);
        }
    }
}
