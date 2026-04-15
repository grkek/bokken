#include "Canvas.hpp"

namespace Bokken
{
    namespace Modules
    {
        std::map<void *, std::vector<JSValue>> Canvas::s_states;
        void *Canvas::s_active_comp = nullptr;
        int Canvas::s_hook_idx = 0;
        JSValue Canvas::s_root_element = JS_UNDEFINED;

        struct StateSetterData
        {
            void *comp_ptr;
            int hook_idx;
        };

        TTF_Font *Canvas::get_font(const std::string &path, float size)
        {
            std::string key = path + ":" + std::to_string((int)size);

            if (s_font_cache.count(key))
                return s_font_cache[key];

            if (!s_assets)
            {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[Canvas] AssetPack not initialized.");
                return nullptr;
            }

            std::string targetPath = path;
            if (targetPath.empty() || !s_assets->exists(targetPath))
                targetPath = "fonts/default.ttf";

            if (!s_assets->exists(targetPath))
            {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[Canvas] Font path does not exist: %s", targetPath.c_str());
                return nullptr;
            }

            SDL_IOStream *fontStream = s_assets->openIOStream(targetPath);
            if (fontStream)
            {
                TTF_Font *font = TTF_OpenFontIO(fontStream, true, size);
                if (font)
                {
                    s_font_cache[key] = font;
                    return font;
                }
            }

            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "[Canvas] Failed to load font %s: %s", targetPath.c_str(), SDL_GetError());
            return nullptr;
        }

        void Canvas::clear_font_cache()
        {
            for (auto const &[key, font] : s_font_cache)
                TTF_CloseFont(font);
            s_font_cache.clear();
        }

        void Canvas::parse_simple_style_sheet(JSContext *ctx, JSValue style, Bokken::Canvas::SimpleStyleSheet &out)
        {
            if (!JS_IsObject(style))
                return;

            // Helper to handle both numeric (100) and string ("100%") values
            auto get_dimension = [&](const char *k, float &val, bool &isPercent)
            {
                JSValue v = JS_GetPropertyStr(ctx, style, k);
                if (JS_IsNumber(v))
                {
                    double d;
                    JS_ToFloat64(ctx, &d, v);
                    val = (float)d;
                    isPercent = false;
                }
                else if (JS_IsString(v))
                {
                    const char *str = JS_ToCString(ctx, v);
                    if (str)
                    {
                        std::string s(str);
                        if (!s.empty() && s.back() == '%')
                        {
                            try
                            {
                                val = std::stof(s.substr(0, s.size() - 1));
                                isPercent = true;
                            }
                            catch (...)
                            {
                                val = 0;
                                isPercent = false;
                            }
                        }
                        else
                        {
                            try
                            {
                                val = std::stof(s);
                                isPercent = false;
                            }
                            catch (...)
                            {
                                val = 0;
                                isPercent = false;
                            }
                        }
                        JS_FreeCString(ctx, str);
                    }
                }
                JS_FreeValue(ctx, v);
            };

            auto get_f = [&](const char *k, float &t)
            {
                JSValue v = JS_GetPropertyStr(ctx, style, k);
                if (JS_IsNumber(v))
                {
                    double d;
                    if (JS_ToFloat64(ctx, &d, v) == 0 && !std::isnan(d))
                        t = (float)d;
                }
                JS_FreeValue(ctx, v);
            };

            auto get_u32 = [&](const char *k, uint32_t &t)
            {
                JSValue v = JS_GetPropertyStr(ctx, style, k);
                uint32_t u;
                if (JS_ToUint32(ctx, &u, v) == 0)
                    t = u;
                JS_FreeValue(ctx, v);
            };

            auto get_s = [&](const char *k, std::string &t)
            {
                JSValue v = JS_GetPropertyStr(ctx, style, k);
                if (JS_IsString(v))
                {
                    const char *str = JS_ToCString(ctx, v);
                    if (str)
                    {
                        t = str;
                        JS_FreeCString(ctx, str);
                    }
                }
                JS_FreeValue(ctx, v);
            };

            get_dimension("width", out.width, out.widthIsPercent);
            get_dimension("height", out.height, out.heightIsPercent);

            // Standard properties
            get_f("padding", out.padding);
            get_f("margin", out.margin);
            get_s("font", out.font);
            get_f("fontSize", out.fontSize);
            get_u32("backgroundColor", out.backgroundColor);
            get_u32("color", out.color);

            // Flex Alignment
            std::string jContent = "start";
            std::string aItems = "start";
            get_s("justifyContent", jContent);
            get_s("alignItems", aItems);

            out.justifyContent = (jContent == "center") ? Bokken::Canvas::Align::Center : (jContent == "end" ? Bokken::Canvas::Align::End : Bokken::Canvas::Align::Start);
            out.alignItems = (aItems == "center") ? Bokken::Canvas::Align::Center : (aItems == "end" ? Bokken::Canvas::Align::End : Bokken::Canvas::Align::Start);
        }

        JSValue Canvas::create_element(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
        {
            JSValue el = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, el, "type", JS_DupValue(ctx, argv[0]));
            JSValue properties = (argc > 1 && JS_IsObject(argv[1])) ? JS_DupValue(ctx, argv[1]) : JS_NewObject(ctx);

            if (argc > 2)
            {
                JSValue children = JS_NewArray(ctx);
                for (int i = 2; i < argc; i++)
                    JS_SetPropertyUint32(ctx, children, i - 2, JS_DupValue(ctx, argv[i]));
                JS_SetPropertyStr(ctx, properties, "children", children);
            }
            JS_SetPropertyStr(ctx, el, "properties", properties);
            return el;
        }

        JSValue Canvas::use_state(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
        {
            if (!s_active_comp)
            {
                return JS_ThrowInternalError(ctx, "Hooks can only be called inside a Component function.");
            }

            auto &states = s_states[s_active_comp];
            int idx = s_hook_idx++;

            // Initialize state if it's the first run
            if (idx >= (int)states.size())
            {
                states.push_back(JS_DupValue(ctx, argv[0]));
            }

            JSValue res = JS_NewArray(ctx);

            // 0: Current Value
            JS_SetPropertyUint32(ctx, res, 0, JS_DupValue(ctx, states[idx]));

            // 1: Setter Function
            // We pack the component pointer and index into atoms/data
            JSValue data[2];
            data[0] = JS_NewInt32(ctx, idx);
            // We use the raw pointer as an "opaque" value; handle with care
            data[1] = JS_NewInt64(ctx, (int64_t)s_active_comp);

            JSValue setter = JS_NewCFunctionData(ctx, [](JSContext *c, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValue *data) -> JSValue
                                                 {
            int32_t idx;
            int64_t comp_ptr_int;
            JS_ToInt32(c, &idx, data[0]);
            JS_ToInt64(c, &comp_ptr_int, data[1]);
            void* comp_ptr = (void*)comp_ptr_int;

            if (argc > 0) {
                auto& st = s_states[comp_ptr];
                // Free old value, store new one
                JS_FreeValue(c, st[idx]);
                st[idx] = JS_DupValue(c, argv[0]);

                // Trigger re-render
                return render(c, JS_UNDEFINED, 1, &s_root_element);
            }
            return JS_UNDEFINED; }, 1, 0, 2, data);

            JS_SetPropertyUint32(ctx, res, 1, setter);

            return res;
        }

        std::shared_ptr<Bokken::Canvas::Node> Canvas::synchronize_tree(JSContext *ctx, JSValue val)
        {
            JSValue type = JS_GetPropertyStr(ctx, val, "type");
            if (JS_IsException(type))
                return nullptr;

            // 1. Handle functional components (React-like)
            if (JS_IsFunction(ctx, type))
            {
                void *prev_comp = s_active_comp;
                int prev_idx = s_hook_idx;
                s_active_comp = JS_VALUE_GET_PTR(type);
                s_hook_idx = 0;

                JSValue properties = JS_GetPropertyStr(ctx, val, "properties");
                JSValue result = JS_Call(ctx, type, JS_UNDEFINED, 1, &properties);

                auto node = synchronize_tree(ctx, result);

                JS_FreeValue(ctx, properties);
                JS_FreeValue(ctx, result);
                JS_FreeValue(ctx, type);
                s_active_comp = prev_comp;
                s_hook_idx = prev_idx;
                return node;
            }

            // 2. Extract primitive metadata
            const char *tStr = JS_ToCString(ctx, type);
            std::string typeName = tStr ? tStr : "Unknown";
            JS_FreeCString(ctx, tStr);
            JS_FreeValue(ctx, type);

            Bokken::Canvas::SimpleStyleSheet extractedStyle;
            std::string extractedText = "";
            std::vector<std::shared_ptr<Bokken::Canvas::Node>> childrenNodes;

            // We store this temporarily until the node is instantiated
            JSValue jsOnClick = JS_UNDEFINED;

            JSValue properties = JS_GetPropertyStr(ctx, val, "properties");
            if (JS_IsObject(properties))
            {
                // Parse Style
                JSValue style = JS_GetPropertyStr(ctx, properties, "style");
                parse_simple_style_sheet(ctx, style, extractedStyle);
                JS_FreeValue(ctx, style);

                // Capture onClick but don't bind yet
                jsOnClick = JS_GetPropertyStr(ctx, properties, "onClick");

                // Parse Children/Text
                JSValue children = JS_GetPropertyStr(ctx, properties, "children");
                if (typeName == "Label")
                {
                    // If it's a Label, we flatten all children into one string (allows "Count: {count}")
                    if (JS_IsString(children) || JS_IsNumber(children))
                    {
                        const char *text = JS_ToCString(ctx, children);
                        extractedText = text ? text : "";
                        JS_FreeCString(ctx, text);
                    }
                    else if (JS_IsArray(children))
                    {
                        uint32_t len = 0;
                        JSValue jsLen = JS_GetPropertyStr(ctx, children, "length");
                        JS_ToUint32(ctx, &len, jsLen);
                        JS_FreeValue(ctx, jsLen);

                        std::string flattened;
                        for (uint32_t i = 0; i < len; i++)
                        {
                            JSValue c = JS_GetPropertyUint32(ctx, children, i);
                            const char *text = JS_ToCString(ctx, c);
                            if (text)
                                flattened += text;
                            JS_FreeCString(ctx, text);
                            JS_FreeValue(ctx, c);
                        }
                        extractedText = flattened;
                    }
                }
                else // It's a View or other container
                {
                    if (JS_IsArray(children))
                    {
                        uint32_t len = 0;
                        JSValue jsLen = JS_GetPropertyStr(ctx, children, "length");
                        JS_ToUint32(ctx, &len, jsLen);
                        JS_FreeValue(ctx, jsLen);

                        for (uint32_t i = 0; i < len; i++)
                        {
                            JSValue c = JS_GetPropertyUint32(ctx, children, i);
                            if (JS_IsObject(c))
                            {
                                auto childNode = synchronize_tree(ctx, c);
                                if (childNode)
                                    childrenNodes.push_back(childNode);
                            }
                            JS_FreeValue(ctx, c);
                        }
                    }
                }
                JS_FreeValue(ctx, children);
            }

            // 3. Create the concrete Node instance
            std::shared_ptr<Bokken::Canvas::Node> node = nullptr;

            if (typeName == "Label")
            {
                Bokken::Canvas::Components::Label labelComponent(extractedText);
                labelComponent.setStyle(extractedStyle);
                node = labelComponent.toNode();
            }
            else if (typeName == "View")
            {
                Bokken::Canvas::Components::View viewComponent;
                viewComponent.setStyle(extractedStyle);
                for (auto &child : childrenNodes)
                {
                    viewComponent.addChild(child);
                }
                node = viewComponent.toNode();
            }
            else
            {
                // Fallback for generic nodes
                node = std::make_shared<Bokken::Canvas::Node>(typeName);
                node->style = extractedStyle;
                node->textContent = extractedText;
                node->children = childrenNodes;
            }

            // 4. Bind Interactions (Now that 'node' is guaranteed to exist)
            if (JS_IsFunction(ctx, jsOnClick))
            {
                // Increase the reference count so JS doesn't delete this while C++ holds it
                JSValue capturedFunction = JS_DupValue(ctx, jsOnClick);

                node->onClick = [ctx, capturedFunction]()
                {
                    // Trigger the JS function!
                    JSValue ret = JS_Call(ctx, capturedFunction, JS_UNDEFINED, 0, nullptr);

                    // Handle potential JS errors (very helpful for debugging!)
                    if (JS_IsException(ret))
                    {
                        JSValue exception = JS_GetException(ctx);
                        const char *str = JS_ToCString(ctx, exception);
                        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[JS Error] %s", str);
                        JS_FreeCString(ctx, str);
                        JS_FreeValue(ctx, exception);
                    }
                    JS_FreeValue(ctx, ret);
                };

                // Important: decrement ref count when C++ node is destroyed to prevent leaks
                node->onDeconstruct = [ctx, capturedFunction]()
                {
                    JS_FreeValue(ctx, capturedFunction);
                };
            }

            // Cleanup local handles
            JS_FreeValue(ctx, jsOnClick);
            JS_FreeValue(ctx, properties);

            return node;
        }

        void drawNode(SDL_Renderer *renderer, std::shared_ptr<Bokken::Canvas::Node> node)
        {
            if (!node || !renderer)
                return;

            if (node->type == "View")
            {
                Bokken::Canvas::Components::View::draw(renderer, node);
            }
            else if (node->type == "Label")
            {
                Bokken::Canvas::Components::Label::draw(renderer, node, Canvas::s_assets);
            }

            for (auto &child : node->children)
            {
                drawNode(renderer, child);
            }
        }

        std::shared_ptr<Bokken::Canvas::Node> Canvas::find_node_at(std::shared_ptr<Bokken::Canvas::Node> root, float mx, float my)
        {
            if (!root)
                return nullptr;

            // Search children in reverse order (top to bottom)
            for (auto it = root->children.rbegin(); it != root->children.rend(); ++it)
            {
                auto hit = find_node_at(*it, mx, my);
                if (hit)
                    return hit;
            }

            // Check self
            if (mx >= root->layout.x && mx <= root->layout.x + root->layout.w &&
                my >= root->layout.y && my <= root->layout.y + root->layout.h)
            {
                return root;
            }

            return nullptr;
        }

        void Canvas::handle_event(const SDL_Event &event)
        {
            if (!s_current_tree)
                return;

            float mx, my;
            SDL_GetMouseState(&mx, &my);
            auto hit = find_node_at(s_current_tree, mx, my);

            if (event.type == SDL_EVENT_MOUSE_MOTION)
            {
                if (hit != s_hovered_node)
                {
                    if (s_hovered_node && s_hovered_node->onMouseLeave)
                        s_hovered_node->onMouseLeave();
                    if (hit && hit->onMouseEnter)
                        hit->onMouseEnter();
                    s_hovered_node = hit;
                }
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
            {
                if (hit)
                {
                    s_pressed_node = hit;
                    // Animation: target a smaller scale for the "squish"
                    hit->targetScale = 0.92f;
                }
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP)
            {
                if (s_pressed_node)
                {
                    s_pressed_node->targetScale = 1.0f; // Return to normal size
                    if (hit == s_pressed_node && s_pressed_node->onClick)
                    {
                        s_pressed_node->onClick();
                    }
                    s_pressed_node = nullptr;
                }
            }
        }

        void Canvas::update_node_animations(std::shared_ptr<Bokken::Canvas::Node> node, float deltaTime)
        {
            if (!node)
                return;

            // Linear Interpolation (Lerp) for the "Squish" effect
            // 15.0f is the speed. Higher = snappier, Lower = floatier.
            float speed = 15.0f * deltaTime;

            // Move visualScale toward targetScale
            node->visualScale += (node->targetScale - node->visualScale) * speed;

            // If the scale is changing, we should mark it for repaint if using complex shaders,
            // though for simple scaling, SDL_RenderTexture handles it every frame anyway.

            // Recurse through children
            for (auto &child : node->children)
            {
                update_node_animations(child, deltaTime);
            }
        }

        void Canvas::present()
        {
            if (!s_renderer || !s_current_tree)
                return;

            int drawW, drawH;
            SDL_GetRenderOutputSize(s_renderer, &drawW, &drawH);

            s_current_tree->layout.x = 0;
            s_current_tree->layout.y = 0;
            s_current_tree->layout.w = static_cast<float>(drawW);
            s_current_tree->layout.h = static_cast<float>(drawH);

            if (s_current_tree->onCompute)
                s_current_tree->onCompute(s_current_tree, s_assets);

            if (s_current_tree->onLayout)
                s_current_tree->onLayout(s_current_tree);

            SDL_SetRenderDrawBlendMode(s_renderer, SDL_BLENDMODE_BLEND);
            drawNode(s_renderer, s_current_tree);
        }

        void Canvas::update(float deltaTime)
        {
            if (s_current_tree)
            {
                update_node_animations(s_current_tree, deltaTime);
            }
        }

        JSValue Canvas::render(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
        {
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[Canvas] Rendering the tree");
            if (argc < 1)
                return JS_UNDEFINED;

            // Update the JS reference to the root element
            if (JS_VALUE_GET_PTR(s_root_element) != JS_VALUE_GET_PTR(argv[0]))
            {
                JS_FreeValue(ctx, s_root_element);
                s_root_element = JS_DupValue(ctx, argv[0]);
            }

            // Build/Update the C++ tree
            s_current_tree = synchronize_tree(ctx, s_root_element);

            if (s_current_tree)
            {
                int w = 0, h = 0;
                if (s_window)
                {
                    SDL_GetWindowSize(s_window, &w, &h);
                }

                // Ensure we don't pass 0 or negative values which might cause div-by-zero
                float targetW = (w > 0) ? (float)w : 800.0f;
                float targetH = (h > 0) ? (float)h : 600.0f;

                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[Canvas] Computing layout for size: %.2f x %.2f", targetW, targetH);

                // Start with clean 0.0 floats
                s_current_tree->compute(0.0f, 0.0f, targetW, targetH, s_assets);
            }
            else
            {
                SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "[Canvas] Unable to synchronize the tree");
            }

            return JS_UNDEFINED;
        }

        int Canvas::declare(JSContext *ctx, JSModuleDef *m)
        {
            JS_AddModuleExport(ctx, m, "default");
            JS_AddModuleExport(ctx, m, "useState");
            JS_AddModuleExport(ctx, m, "Align");
            JS_AddModuleExport(ctx, m, "View");
            JS_AddModuleExport(ctx, m, "Label");
            return 0;
        }

        int Canvas::init(JSContext *ctx, JSModuleDef *m)
        {
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[Canvas] Initializing module");
            JSValue defaultExport = JS_NewObject(ctx);

            JS_SetPropertyStr(ctx, defaultExport, "createElement",
                              JS_NewCFunction(ctx, create_element, "createElement", 3));
            JS_SetPropertyStr(ctx, defaultExport, "render",
                              JS_NewCFunction(ctx, render, "render", 1));

            // Align enum
            JSValue align = JS_NewObject(ctx);

            JS_SetPropertyStr(ctx, align, "start", JS_NewString(ctx, "start"));
            JS_SetPropertyStr(ctx, align, "center", JS_NewString(ctx, "center"));
            JS_SetPropertyStr(ctx, align, "end", JS_NewString(ctx, "end"));

            JS_SetModuleExport(ctx, m, "Align", align);
            JS_SetModuleExport(ctx, m, "View", JS_NewString(ctx, "View"));
            JS_SetModuleExport(ctx, m, "Label", JS_NewString(ctx, "Label"));
            JS_SetModuleExport(ctx, m, "useState", JS_NewCFunction(ctx, use_state, "useState", 1));
            JS_SetModuleExport(ctx, m, "default", defaultExport);

            return 0;
        }
    }
}