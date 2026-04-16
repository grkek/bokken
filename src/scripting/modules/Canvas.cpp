#include "Canvas.hpp"

namespace Bokken
{
    namespace Scripting
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

            struct JSCapture
            {
                JSContext *ctx;
                JSValue val;
                JSCapture(JSContext *c, JSValue v) : ctx(c), val(v) {}
                ~JSCapture() { JS_FreeValue(ctx, val); } // Stops the QuickJS GC Segfault
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

                // Core Dimensions
                get_dimension("width", out.width, out.widthIsPercent);
                get_dimension("height", out.height, out.heightIsPercent);

                // Box Model (Margins)
                get_f("margin", out.margin);
                get_f("marginTop", out.marginTop);
                get_f("marginBottom", out.marginBottom);
                get_f("marginLeft", out.marginLeft);
                get_f("marginRight", out.marginRight);

                // Box Model (Paddings)
                get_f("padding", out.padding);
                get_f("paddingTop", out.paddingTop);
                get_f("paddingBottom", out.paddingBottom);
                get_f("paddingLeft", out.paddingLeft);
                get_f("paddingRight", out.paddingRight);

                // Positioning
                get_f("top", out.top);
                get_f("bottom", out.bottom);
                get_f("left", out.left);
                get_f("right", out.right);

                JSValue zVal = JS_GetPropertyStr(ctx, style, "zIndex");
                if (JS_IsNumber(zVal))
                {
                    int32_t z;
                    JS_ToInt32(ctx, &z, zVal);
                    out.zIndex = z;
                }
                JS_FreeValue(ctx, zVal);

                std::string posStr = "Relative";
                get_s("position", posStr);
                out.position = (posStr == "Absolute") ? Bokken::Canvas::Position::Absolute : Bokken::Canvas::Position::Relative;

                // Visuals & Borders
                get_u32("backgroundColor", out.backgroundColor);
                get_u32("color", out.color);
                get_f("opacity", out.opacity);
                get_f("borderRadius", out.borderRadius);
                get_f("borderWidth", out.borderWidth);
                get_u32("borderColor", out.borderColor);

                std::string overflowStr = "Visible";
                get_s("overflow", overflowStr);
                out.overflow = (overflowStr == "Hidden") ? Bokken::Canvas::Overflow::Hidden : Bokken::Canvas::Overflow::Visible;

                // Text
                get_s("font", out.font);
                get_f("fontSize", out.fontSize);

                // Flex Layout
                std::string flexDir = "Column";
                get_s("flexDirection", flexDir);
                out.flexDirection = (flexDir == "Row") ? Bokken::Canvas::FlexDirection::Row : Bokken::Canvas::FlexDirection::Column;
                get_f("flex", out.flex);

                std::string jContent = "Start";
                std::string aItems = "Center";
                get_s("justifyContent", jContent);
                get_s("alignItems", aItems);

                out.justifyContent = (jContent == "Center") ? Bokken::Canvas::Align::Center : (jContent == "End" ? Bokken::Canvas::Align::End : Bokken::Canvas::Align::Start);
                out.alignItems = (aItems == "Center") ? Bokken::Canvas::Align::Center : (aItems == "End" ? Bokken::Canvas::Align::End : Bokken::Canvas::Align::Start);

                // Animation & Interaction
                get_f("transitionDuration", out.transitionDuration);
                get_f("hoverScale", out.hoverScale);
                get_f("activeScale", out.activeScale);

                std::string timingStr = "Linear";
                get_s("transitionTiming", timingStr);

                if (timingStr == "EaseIn")
                    out.transitionTiming = Bokken::Canvas::Timing::EaseIn;
                else if (timingStr == "EaseOut")
                    out.transitionTiming = Bokken::Canvas::Timing::EaseOut;
                else if (timingStr == "EaseInOut")
                    out.transitionTiming = Bokken::Canvas::Timing::EaseInOut;
                else if (timingStr == "Bounce")
                    out.transitionTiming = Bokken::Canvas::Timing::Bounce;
                else if (timingStr == "Back")
                    out.transitionTiming = Bokken::Canvas::Timing::Back;
                else if (timingStr == "Step")
                    out.transitionTiming = Bokken::Canvas::Timing::Step;
                else
                    out.transitionTiming = Bokken::Canvas::Timing::Linear;
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
                auto &engine = Bokken::Scripting::Engine::Instance();

                // 1. Validate 'type' property
                JSValue type = JS_GetPropertyStr(ctx, val, "type");
                if (JS_IsException(type))
                {
                    return nullptr;
                }

                if (JS_IsFunction(ctx, type))
                {
                    void *prev_comp = s_active_comp;
                    int prev_idx = s_hook_idx;

                    s_active_comp = JS_VALUE_GET_PTR(type);
                    s_hook_idx = 0;

                    JSValue properties = JS_GetPropertyStr(ctx, val, "properties");
                    JSValue result = JS_Call(ctx, type, JS_UNDEFINED, 1, &properties);

                    if (JS_IsException(result))
                    {
                        engine.reportException("Unable to synchronize a functional component");
                        JS_FreeValue(ctx, properties);
                        JS_FreeValue(ctx, type);
                        return nullptr;
                    }

                    // Recursively build the tree from the component's output
                    auto node = synchronize_tree(ctx, result);

                    JS_FreeValue(ctx, properties);
                    JS_FreeValue(ctx, result);
                    JS_FreeValue(ctx, type);

                    s_active_comp = prev_comp;
                    s_hook_idx = prev_idx;
                    return node;
                }

                const char *tStr = JS_ToCString(ctx, type);
                std::string typeName = tStr ? tStr : "View";
                JS_FreeCString(ctx, tStr);
                JS_FreeValue(ctx, type);

                auto node = std::make_shared<Bokken::Canvas::Node>(typeName);
                JSValue properties = JS_GetPropertyStr(ctx, val, "properties");

                // Exit early if no properties exist
                if (!JS_IsObject(properties))
                {
                    JS_FreeValue(ctx, properties);
                    return node;
                }

                // 2. Parse Styles
                JSValue style = JS_GetPropertyStr(ctx, properties, "style");
                parse_simple_style_sheet(ctx, style, node->style);
                JS_FreeValue(ctx, style);

                // 3. Process Children
                JSValue children = JS_GetPropertyStr(ctx, properties, "children");

                if (typeName == "Label")
                {
                    // Handle text content for Labels
                    if (JS_IsString(children) || JS_IsNumber(children) || JS_IsBool(children))
                    {
                        JSValue strVal = JS_ToString(ctx, children);
                        if (!JS_IsException(strVal))
                        {
                            const char *text = JS_ToCString(ctx, strVal);
                            node->textContent = text ? text : "";
                            JS_FreeCString(ctx, text);
                        }
                        JS_FreeValue(ctx, strVal);
                    }
                    else if (JS_IsArray(children))
                    {
                        uint32_t len = 0;
                        JSValue jsLen = JS_GetPropertyStr(ctx, children, "length");
                        JS_ToUint32(ctx, &len, jsLen);
                        JS_FreeValue(ctx, jsLen);

                        std::string flattened = "";

                        for (uint32_t i = 0; i < len; i++)
                        {
                            JSValue c = JS_GetPropertyUint32(ctx, children, i);

                            // Convert ANY JS value → string safely
                            JSValue strVal = JS_ToString(ctx, c);
                            if (!JS_IsException(strVal))
                            {
                                const char *text = JS_ToCString(ctx, strVal);
                                if (text)
                                {
                                    flattened += text;
                                    JS_FreeCString(ctx, text);
                                }
                            }

                            JS_FreeValue(ctx, strVal);
                            JS_FreeValue(ctx, c);
                        }

                        node->textContent = flattened;
                    }
                }
                else if (JS_IsArray(children))
                {
                    // Recursively add children for non-label nodes
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
                                node->add_child(childNode);
                        }
                        JS_FreeValue(ctx, c);
                    }
                }
                JS_FreeValue(ctx, children);

                // 4. Bind Events (onClick)
                JSValue jsOnClick = JS_GetPropertyStr(ctx, properties, "onClick");
                if (JS_IsFunction(ctx, jsOnClick))
                {
                    // Reference count increment to keep the function alive in JS heap
                    JSValue capturedVal = JS_DupValue(ctx, jsOnClick);

                    // NATIVE LAMBDA: Triggers when the user interacts with the C++ Node
                    node->onClick = [capturedVal]()
                    {
                        auto &engine = Bokken::Scripting::Engine::Instance();
                        if (!engine.isReady())
                        {
                            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Bokken Warning: onClick skipped because Engine is NOT ready!");
                            return;
                        }

                        if (engine.isReady())
                        {
                            JSContext *liveCtx = engine.context();
                            JSValue ret = JS_Call(liveCtx, capturedVal, JS_UNDEFINED, 0, nullptr);

                            if (JS_IsException(ret))
                            {
                                engine.reportException("onClick Handler");
                            }
                            JS_FreeValue(liveCtx, ret);
                        }
                    };

                    node->onDeconstruct = [capturedVal]()
                    {
                        auto &engine = Bokken::Scripting::Engine::Instance();
                        if (engine.isReady())
                        {
                            JS_FreeValue(engine.context(), capturedVal);
                        }
                    };
                }

                // Cleanup and return
                JS_FreeValue(ctx, jsOnClick);
                JS_FreeValue(ctx, properties);

                if (typeName == "View")
                {
                    node->onCompute = &Bokken::Canvas::Components::View::computeNode;
                    node->onLayout = &Bokken::Canvas::Components::View::layoutNode;
                }
                else if (typeName == "Label")
                {
                    node->onCompute = &Bokken::Canvas::Components::Label::computeNode;
                }

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

            std::shared_ptr<Bokken::Canvas::Node> Canvas::find_node_at(std::shared_ptr<Bokken::Canvas::Node> root, float x, float y)
            {
                if (!root)
                    return nullptr;

                /* 1. Search children first (Deepest/Top-most elements) */
                for (auto it = root->children.rbegin(); it != root->children.rend(); ++it)
                {
                    auto hit = find_node_at(*it, x, y);
                    if (hit)
                        return hit;
                }

                /* 2. Check if the point is within this specific node's box */
                bool isInside = (x >= root->layout.x && (x <= root->layout.x + root->layout.w) &&
                                 y >= root->layout.y && (y <= root->layout.y + root->layout.h));

                if (isInside)
                {
                    /* 3. Bubble up: Find the responsible 'Interactive' ancestor */
                    // We start with a shared_ptr to the current leaf node
                    std::shared_ptr<Bokken::Canvas::Node> current = root;

                    while (current)
                    {
                        if (current->onClick != nullptr)
                        {
                            return current; /* Found the actual button owner */
                        }

                        /* Move up the chain using the raw parent pointer */
                        if (current->parent)
                        {
                            /* We wrap the raw parent pointer into a temporary shared_ptr.
                               The no-op deleter '[](Bokken::Canvas::Node*){}' is CRITICAL here
                               because the parent is owned by its own parent/the tree,
                               not by this temporary pointer. */
                            current = std::shared_ptr<Bokken::Canvas::Node>(current->parent, [](Bokken::Canvas::Node *) {});
                        }
                        else
                        {
                            current = nullptr;
                        }
                    }
                }

                /* No interactive ancestor found */
                return nullptr;
            }

            void Canvas::markLabelsDirty(std::shared_ptr<Bokken::Canvas::Node> node)
            {
                if (!node)
                    return;
                if (node->type == "Label")
                    node->needsRepaint = true;
                for (auto &child : node->children)
                    markLabelsDirty(child);
            }

            void Canvas::forceRelayout()
            {
                if (!s_current_tree)
                    return;

                markLabelsDirty(s_current_tree);

                int w, h;

                SDL_GetWindowSize(s_window, &w, &h);

                s_current_tree->compute(0, 0, (float)w, (float)h, s_assets);

                if (s_current_tree->onLayout)
                    s_current_tree->onLayout(s_current_tree);
            }

            void Canvas::handleEvent(const SDL_Event &e)
            {
                if (!s_current_tree)
                    return;

                if (e.type == SDL_EVENT_WINDOW_RESIZED ||
                    e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
                    e.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED)
                {
                    forceRelayout();
                    return;
                }

                if (e.type == SDL_EVENT_WINDOW_MOUSE_LEAVE)
                {
                    if (s_hovered_node)
                        s_hovered_node->isHovered = false;

                    s_hovered_node = nullptr;
                }

                if (e.type == SDL_EVENT_MOUSE_MOTION)
                {
                    auto hit = find_node_at(s_current_tree, e.motion.x, e.motion.y);

                    if (hit != s_hovered_node)
                    {
                        if (s_hovered_node)
                            s_hovered_node->isHovered = false;
                        s_hovered_node = hit;
                        if (s_hovered_node)
                            s_hovered_node->isHovered = true;
                    }
                }

                if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
                {
                    s_pressed_node = find_node_at(s_current_tree, e.button.x, e.button.y);
                    if (s_pressed_node)
                        s_pressed_node->isActive = true;
                }

                if (e.type == SDL_EVENT_MOUSE_BUTTON_UP)
                {
                    auto hit = find_node_at(s_current_tree, e.button.x, e.button.y);

                    std::shared_ptr<Bokken::Canvas::Node> keeper = hit;

                    if (keeper && keeper == s_pressed_node)
                    {
                        if (keeper->onClick)
                        {
                            keeper->onClick();
                        }
                    }

                    if (s_pressed_node)
                        s_pressed_node->isActive = false;
                    s_pressed_node = nullptr;

                    if (s_hovered_node)
                        s_hovered_node->isHovered = false;
                    s_hovered_node = nullptr;

                    s_hovered_node = find_node_at(s_current_tree, e.button.x, e.button.y);
                    if (s_hovered_node)
                        s_hovered_node->isHovered = true;
                }
            }

            void Canvas::update_node_animations(std::shared_ptr<Bokken::Canvas::Node> node, float deltaTime)
            {
                if (!node)
                    return;

                // 1. Determine the target scale for this node
                float goal = 1.0f;

                // We check for onClick to see if it's an interactive component (Button/View)
                if (node->onClick != nullptr)
                {
                    if (node->isActive)
                        goal = node->style.activeScale;
                    else if (node->isHovered)
                        goal = node->style.hoverScale;
                    else
                        goal = 1.0f;
                }
                else if (node->parent != nullptr)
                {
                    // SAFETY: Only inherit from parent if the parent is actually still part
                    // of the active tree. Since parent is a raw pointer, this is a risk.
                    // We assume here that parent is managed by the shared_ptr of the tree.
                    goal = node->parent->visualScale;
                }

                // 2. Detect Change: If the goal changed, reset the lerp start point
                if (std::abs(goal - node->targetScale) > 0.001f)
                {
                    node->startScale = node->visualScale;
                    node->targetScale = goal;
                    node->animationTimer = 0.0f;
                }

                // 3. Process the Animation Lerp
                if (node->style.transitionDuration <= 0.0f)
                {
                    node->visualScale = node->targetScale;
                }
                else if (node->animationTimer < 1.0f)
                {
                    // Prevent division by zero just in case
                    float duration = std::max(node->style.transitionDuration, 0.0001f);
                    node->animationTimer += deltaTime / duration;

                    if (node->animationTimer > 1.0f)
                        node->animationTimer = 1.0f;

                    float t = apply_easing(node->style.transitionTiming, node->animationTimer);
                    node->visualScale = node->startScale + (node->targetScale - node->startScale) * t;
                }
                else
                {
                    // Ensure we snap to exact target when timer finishes
                    node->visualScale = node->targetScale;
                }

                // 4. Recursive Pass: Update children
                // Using a local copy of children or standard iterator to ensure
                // we don't crash if the tree is modified during the walk.
                for (const auto &child : node->children)
                {
                    if (child)
                    {
                        update_node_animations(child, deltaTime);
                    }
                }
            }

            void Canvas::reset_active_states(std::shared_ptr<Bokken::Canvas::Node> node)
            {
                if (!node)
                    return;
                node->isActive = false;
                for (auto &child : node->children)
                    reset_active_states(child);
            }

            float Canvas::apply_easing(Bokken::Canvas::Timing func, float t)
            {
                switch (func)
                {
                case Bokken::Canvas::Timing::EaseIn:
                    return t * t; /* Quadratic */
                case Bokken::Canvas::Timing::EaseOut:
                    return t * (2 - t);
                case Bokken::Canvas::Timing::EaseInOut:
                    return t < 0.5f ? 2 * t * t : -1 + (4 - 2 * t) * t;
                case Bokken::Canvas::Timing::Bounce:
                    /* Quick & dirty bounce */
                    if (t < (1 / 2.75f))
                        return 7.5625f * t * t;
                    return 1.0f; /* simplified */
                case Bokken::Canvas::Timing::Linear:
                default:
                    return t;
                }
            }

            void Canvas::present()
            {
                if (!s_renderer || !s_current_tree)
                    return;

                int drawW, drawH;
                SDL_GetRenderOutputSize(s_renderer, &drawW, &drawH);

                SDL_SetRenderDrawBlendMode(s_renderer, SDL_BLENDMODE_BLEND);
                drawNode(s_renderer, s_current_tree);
            }

            void Canvas::update(float deltaTime)
            {
                auto tree = s_current_tree;

                if (tree)
                {
                    update_node_animations(tree, deltaTime);
                }
            }

            JSValue Canvas::render(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
            {
                if (argc < 1)
                    return JS_UNDEFINED;

                s_hovered_node = nullptr;
                s_pressed_node = nullptr;

                JSValue nextRoot = JS_DupValue(ctx, argv[0]);
                JS_FreeValue(ctx, s_root_element);
                s_root_element = nextRoot;

                auto new_tree = synchronize_tree(ctx, s_root_element);
                s_current_tree = new_tree;

                if (s_current_tree)
                {
                    int w, h;
                    SDL_GetWindowSize(s_window, &w, &h);

                    s_current_tree->compute(0, 0, (float)w, (float)h, s_assets);

                    if (s_current_tree->onLayout)
                        s_current_tree->onLayout(s_current_tree);
                }

                return JS_UNDEFINED;
            }

            int Canvas::declare(JSContext *ctx, JSModuleDef *m)
            {
                JS_AddModuleExport(ctx, m, "default");
                JS_AddModuleExport(ctx, m, "useState");

                JS_AddModuleExport(ctx, m, "Align");
                JS_AddModuleExport(ctx, m, "FlexDirection");
                JS_AddModuleExport(ctx, m, "Label");
                JS_AddModuleExport(ctx, m, "Overflow");
                JS_AddModuleExport(ctx, m, "Position");
                JS_AddModuleExport(ctx, m, "Timing");
                JS_AddModuleExport(ctx, m, "View");

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

                JS_SetPropertyStr(ctx, align, "Start", JS_NewString(ctx, "Start"));
                JS_SetPropertyStr(ctx, align, "Center", JS_NewString(ctx, "Center"));
                JS_SetPropertyStr(ctx, align, "End", JS_NewString(ctx, "End"));

                // FlexDirection enum
                JSValue flexDirection = JS_NewObject(ctx);

                JS_SetPropertyStr(ctx, flexDirection, "Row", JS_NewString(ctx, "Row"));
                JS_SetPropertyStr(ctx, flexDirection, "Column", JS_NewString(ctx, "Column"));

                // Overflow enum
                JSValue overflow = JS_NewObject(ctx);

                JS_SetPropertyStr(ctx, overflow, "Visible", JS_NewString(ctx, "Visible"));
                JS_SetPropertyStr(ctx, overflow, "Hidden", JS_NewString(ctx, "Hidden"));

                // Position enum
                JSValue position = JS_NewObject(ctx);

                JS_SetPropertyStr(ctx, position, "Relative", JS_NewString(ctx, "Relative"));
                JS_SetPropertyStr(ctx, position, "Absolute", JS_NewString(ctx, "Absolute"));

                // Timing enum
                JSValue timing = JS_NewObject(ctx);

                JS_SetPropertyStr(ctx, timing, "Linear", JS_NewString(ctx, "Linear"));
                JS_SetPropertyStr(ctx, timing, "EaseIn", JS_NewString(ctx, "EaseIn"));
                JS_SetPropertyStr(ctx, timing, "EaseOut", JS_NewString(ctx, "EaseOut"));
                JS_SetPropertyStr(ctx, timing, "Bounce", JS_NewString(ctx, "Bounce"));

                JS_SetModuleExport(ctx, m, "Align", align);
                JS_SetModuleExport(ctx, m, "default", defaultExport);
                JS_SetModuleExport(ctx, m, "FlexDirection", flexDirection);
                JS_SetModuleExport(ctx, m, "Label", JS_NewString(ctx, "Label"));
                JS_SetModuleExport(ctx, m, "Overflow", overflow);
                JS_SetModuleExport(ctx, m, "Position", position);
                JS_SetModuleExport(ctx, m, "Timing", timing);
                JS_SetModuleExport(ctx, m, "useState", JS_NewCFunction(ctx, use_state, "useState", 1));
                JS_SetModuleExport(ctx, m, "View", JS_NewString(ctx, "View"));

                return 0;
            }
        }
    }
}