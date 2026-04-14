#pragma once

#include "Base.hpp"
#include <vector>
#include <string>
#include <memory>
#include <map>

namespace Bokken
{
    namespace Modules
    {
        namespace Properties
        {
            struct SimpleStyleSheet
            {
                std::string flexDirection = "column";
                float width = -1.0f, height = -1.0f;
                float padding = 0.0f, margin = 0.0f;
                uint32_t backgroundColor = 0x00000000;
                uint32_t color = 0xFFFFFFFF;
                float fontSize = 16.0f;
                float opacity = 1.0f;
                float borderRadius = 0.0f;
            };

            struct Rect
            {
                float x, y, w, h;
            };

            class Node
            {
            public:
                std::string type;
                SimpleStyleSheet style;
                Rect layout{0, 0, 0, 0};
                std::string textContent;
                std::vector<std::shared_ptr<Node>> children;

                Node(std::string t) : type(std::move(t)) {}

                void compute(float px, float py, float pw, float ph)
                {
                    layout.x = px + style.margin;
                    layout.y = py + style.margin;
                    layout.w = (style.width > 0) ? style.width : pw - (style.margin * 2);
                    layout.h = (style.height > 0) ? style.height : ph - (style.margin * 2);

                    float cx = layout.x + style.padding;
                    float cy = layout.y + style.padding;

                    for (auto &child : children)
                    {
                        child->compute(cx, cy, layout.w - (style.padding * 2), layout.h - (style.padding * 2));
                        if (style.flexDirection == "column")
                            cy += child->layout.h + child->style.margin;
                        else if (style.flexDirection == "row")
                            cx += child->layout.w + child->style.margin;
                    }
                }
            };
        }

        class Canvas : public Base
        {
        public:
            Canvas() : Base("bokken/canvas") {}

            int declare(JSContext *ctx, JSModuleDef *m) override;
            int init(JSContext *ctx, JSModuleDef *m) override;

        private:
            static JSValue create_element(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
            static JSValue render(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
            static JSValue use_state(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

            static std::shared_ptr<Properties::Node> synchronize_tree(JSContext *ctx, JSValue val);
            static void parse_simple_style_sheet(JSContext *ctx, JSValue style, Properties::SimpleStyleSheet &out);

            static std::map<void *, std::vector<JSValue>> s_states;
            static void *s_active_comp;
            static int s_hook_idx;
            static JSValue s_root_element;
        };
    }
}