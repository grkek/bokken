#include "Canvas.hpp"

namespace Bokken
{
    namespace Modules
    {
        std::map<void *, std::vector<JSValue>> Canvas::s_states;
        void *Canvas::s_active_comp = nullptr;
        int Canvas::s_hook_idx = 0;
        JSValue Canvas::s_root_element = JS_UNDEFINED;

        void Canvas::parse_simple_style_sheet(JSContext *ctx, JSValue style, Properties::SimpleStyleSheet &out)
        {
            if (!JS_IsObject(style))
                return;
            auto get_f = [&](const char *k, float &t)
            {
                JSValue v = JS_GetPropertyStr(ctx, style, k);
                double d;
                if (JS_ToFloat64(ctx, &d, v) == 0)
                    t = (float)d;
                JS_FreeValue(ctx, v);
            };
            get_f("width", out.width);
            get_f("height", out.height);
            get_f("padding", out.padding);
            get_f("margin", out.margin);
        }

        JSValue Canvas::create_element(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
        {
            JSValue el = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, el, "type", JS_DupValue(ctx, argv[0]));
            JSValue props = (argc > 1 && JS_IsObject(argv[1])) ? JS_DupValue(ctx, argv[1]) : JS_NewObject(ctx);

            if (argc > 2)
            {
                JSValue children = JS_NewArray(ctx);
                for (int i = 2; i < argc; i++)
                    JS_SetPropertyUint32(ctx, children, i - 2, JS_DupValue(ctx, argv[i]));
                JS_SetPropertyStr(ctx, props, "children", children);
            }
            JS_SetPropertyStr(ctx, el, "props", props);
            return el;
        }

        JSValue Canvas::use_state(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
        {
            if (!s_active_comp)
                return JS_EXCEPTION;
            auto &states = s_states[s_active_comp];
            int idx = s_hook_idx++;
            if (idx >= states.size())
                states.push_back(JS_DupValue(ctx, argv[0]));

            JSValue res = JS_NewArray(ctx);
            JS_SetPropertyUint32(ctx, res, 0, JS_DupValue(ctx, states[idx]));
            JS_SetPropertyUint32(ctx, res, 1, JS_NewCFunction(ctx, [](JSContext *c, JSValueConst t, int a, JSValueConst *av)
                                                              { return render(c, JS_UNDEFINED, 1, &s_root_element); }, "setState", 1));
            return res;
        }

        std::shared_ptr<Properties::Node> Canvas::synchronize_tree(JSContext *ctx, JSValue val)
        {
            JSValue type = JS_GetPropertyStr(ctx, val, "type");

            if (JS_IsFunction(ctx, type))
            {
                s_active_comp = JS_VALUE_GET_PTR(type);
                s_hook_idx = 0;
                JSValue props = JS_GetPropertyStr(ctx, val, "props");
                JSValue result = JS_Call(ctx, type, JS_UNDEFINED, 1, &props);
                auto node = synchronize_tree(ctx, result);
                JS_FreeValue(ctx, props);
                JS_FreeValue(ctx, result);
                JS_FreeValue(ctx, type);
                return node;
            }

            const char *tStr = JS_ToCString(ctx, type);
            auto node = std::make_shared<Properties::Node>(tStr ? tStr : "view");
            JS_FreeCString(ctx, tStr);
            JS_FreeValue(ctx, type);

            JSValue props = JS_GetPropertyStr(ctx, val, "props");
            if (JS_IsObject(props))
            {
                JSValue style = JS_GetPropertyStr(ctx, props, "style");
                parse_simple_style_sheet(ctx, style, node->style);
                JS_FreeValue(ctx, style);

                JSValue children = JS_GetPropertyStr(ctx, props, "children");
                if (JS_IsString(children))
                {
                    node->textContent = JS_ToCString(ctx, children);
                }
                else if (JS_IsArray(children))
                {
                    uint32_t len = 0;
                    JSValue jsLen = JS_GetPropertyStr(ctx, children, "length");
                    JS_ToUint32(ctx, &len, jsLen);
                    for (uint32_t i = 0; i < len; i++)
                    {
                        JSValue c = JS_GetPropertyUint32(ctx, children, i);
                        if (JS_IsObject(c))
                            node->children.push_back(synchronize_tree(ctx, c));
                        JS_FreeValue(ctx, c);
                    }
                    JS_FreeValue(ctx, jsLen);
                }
                JS_FreeValue(ctx, children);
            }
            JS_FreeValue(ctx, props);
            return node;
        }

        JSValue Canvas::render(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
        {
            if (argc < 1)
                return JS_UNDEFINED;
            if (JS_VALUE_GET_PTR(s_root_element) != JS_VALUE_GET_PTR(argv[0]))
            {
                JS_FreeValue(ctx, s_root_element);
                s_root_element = JS_DupValue(ctx, argv[0]);
            }
            auto root = synchronize_tree(ctx, s_root_element);
            root->compute(0, 0, 1280, 720);
            return JS_UNDEFINED;
        }

        int Canvas::declare(JSContext *ctx, JSModuleDef *m)
        {
            JS_AddModuleExport(ctx, m, "default");
            JS_AddModuleExport(ctx, m, "View");
            JS_AddModuleExport(ctx, m, "Label");
            return 0;
        }

        int Canvas::init(JSContext *ctx, JSModuleDef *m)
        {
            JSValue defaultExport = JS_NewObject(ctx);

            JS_SetPropertyStr(ctx, defaultExport, "createElement",
                              JS_NewCFunction(ctx, create_element, "createElement", 3));
            JS_SetPropertyStr(ctx, defaultExport, "render",
                              JS_NewCFunction(ctx, render, "render", 1));
            JS_SetPropertyStr(ctx, defaultExport, "useState",
                              JS_NewCFunction(ctx, use_state, "useState", 1));

            JS_SetModuleExport(ctx, m, "View",
                               JS_NewString(ctx, "view"));
            JS_SetModuleExport(ctx, m, "Label",
                               JS_NewString(ctx, "label"));

            JS_SetModuleExport(ctx, m, "default", defaultExport);

            return 0;
        }
    }
}