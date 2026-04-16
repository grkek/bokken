#pragma once

#include "Base.hpp"
#include "../Engine.hpp"
#include "../../canvas/Align.hpp"
#include "../../canvas/Timing.hpp"
#include "../../canvas/SimpleStyleSheet.hpp"
#include "../../canvas/Node.hpp"
#include "../../canvas/components/Label.hpp"
#include "../../canvas/components/View.hpp"
#include "../../AssetPack.hpp"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <map>

namespace Bokken
{
    namespace Scripting
    {
        namespace Modules
        {
            class Canvas : public Base
            {
            public:
                Canvas(SDL_Window *window, SDL_Renderer *renderer, Bokken::AssetPack *assets)
                    : Base("bokken/canvas"), m_assets(assets)
                {
                    s_window = window;
                    s_renderer = renderer;
                    s_assets = assets;
                }

                int declare(JSContext *ctx, JSModuleDef *m) override;
                int init(JSContext *ctx, JSModuleDef *m) override;

                static TTF_Font *get_font(const std::string &path, float size);
                static void clear_font_cache();

                /** Processes layout animations and input states **/
                static void update(float deltaTime);

                /** Renders the current UI tree to the screen **/
                static void present();

                /** Mark for re-render **/
                static void markLabelsDirty(std::shared_ptr<Bokken::Canvas::Node> node);

                /** Force a layout redraw through events **/
                static void forceRelayout();

                /** Handles SDL events for the UI (clicks, hovers) **/
                static void handleEvent(const SDL_Event &event);

            private:
                friend void drawNode(SDL_Renderer *renderer, std::shared_ptr<Bokken::Canvas::Node> node);

                /** Finds the top-most node under the mouse coordinates **/
                static std::shared_ptr<Bokken::Canvas::Node> find_node_at(std::shared_ptr<Bokken::Canvas::Node> root, float mx, float my);

                /** Recursively updates animation scales (lerping) **/
                static void update_node_animations(std::shared_ptr<Bokken::Canvas::Node> node, float deltaTime);

                /** Applies mathematical easing curves to a normalized time value **/
                static float apply_easing(Bokken::Canvas::Timing func, float t);

                /** Recursively clears active/pressed flags from the tree **/
                static void reset_active_states(std::shared_ptr<Bokken::Canvas::Node> node);

                static inline SDL_Window *s_window = nullptr;
                static inline SDL_Renderer *s_renderer = nullptr;

                Bokken::AssetPack *m_assets;
                static inline Bokken::AssetPack *s_assets = nullptr;

                static inline std::map<std::string, TTF_Font *> s_font_cache;

                /* Tracking the node currently under the mouse */
                static inline std::shared_ptr<Bokken::Canvas::Node> s_hovered_node = nullptr;
                /* Tracking the node currently being pressed */
                static inline std::shared_ptr<Bokken::Canvas::Node> s_pressed_node = nullptr;

                /* QuickJS Module Functions */
                static JSValue create_element(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue render(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue use_state(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

                static std::shared_ptr<Bokken::Canvas::Node> synchronize_tree(JSContext *ctx, JSValue val);
                static void parse_simple_style_sheet(JSContext *ctx, JSValue style, Bokken::Canvas::SimpleStyleSheet &out);

                static inline std::shared_ptr<Bokken::Canvas::Node> s_current_tree = nullptr;
                static std::map<void *, std::vector<JSValue>> s_states;
                static void *s_active_comp;
                static int s_hook_idx;
                static JSValue s_root_element;
            };

            void drawNode(SDL_Renderer *renderer, std::shared_ptr<Bokken::Canvas::Node> node);
        }
    }
}