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
    namespace Renderer { class SpriteBatcher; }

    namespace Scripting
    {
        namespace Modules
        {
            class Canvas : public Base
            {
            public:
                /**
                 * Construct the bokken/canvas scripting module.
                 *
                 * After the GL refactor we hold a SpriteBatcher* (set later by
                 * the Renderer module) instead of an SDL_Renderer. Window
                 * survives because layout still queries window size for
                 * percentage-based units.
                 */
                Canvas(SDL_Window *window, AssetPack *assets)
                    : Base("bokken/canvas"), m_assets(assets)
                {
                    s_window = window;
                    s_assets = assets;
                }

                // Wired in by the Renderer module before the first frame.
                static void setBatcher(Bokken::Renderer::SpriteBatcher *b) { s_batcher = b; }

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
                friend void drawNode(Bokken::Renderer::SpriteBatcher &batcher,
                                      std::shared_ptr<Bokken::Canvas::Node> node,
                                      int layer);

                /** Finds the top-most node under the mouse coordinates **/
                static std::shared_ptr<Bokken::Canvas::Node> find_node_at(std::shared_ptr<Bokken::Canvas::Node> root, float mx, float my);

                /** Recursively updates animation scales (lerping) **/
                static void update_node_animations(std::shared_ptr<Bokken::Canvas::Node> node, float deltaTime);

                /** Applies mathematical easing curves to a normalized time value **/
                static float apply_easing(Bokken::Canvas::Timing func, float t);

                /** Recursively clears active/pressed flags from the tree **/
                static void reset_active_states(std::shared_ptr<Bokken::Canvas::Node> node);

                static inline SDL_Window *s_window = nullptr;
                static inline Bokken::Renderer::SpriteBatcher *s_batcher = nullptr;

                AssetPack *m_assets;
                static inline AssetPack *s_assets = nullptr;

                static inline std::map<std::string, TTF_Font *> s_font_cache;

                /* Tracking the node currently under the mouse */
                static inline std::shared_ptr<Bokken::Canvas::Node> s_hovered_node = nullptr;
                /* Tracking the node currently being pressed */
                static inline std::shared_ptr<Bokken::Canvas::Node> s_pressed_node = nullptr;

                /* QuickJS Module Functions */
                static JSValue create_element(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue render(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue use_state(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue use_effect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

                // Runs queued effects after render() commits the tree.
                static void flush_effects(JSContext *ctx);

                static std::shared_ptr<Bokken::Canvas::Node> synchronize_tree(JSContext *ctx, JSValue val);
                static void parse_simple_style_sheet(JSContext *ctx, JSValue style, Bokken::Canvas::SimpleStyleSheet &out);

                static inline std::shared_ptr<Bokken::Canvas::Node> s_current_tree = nullptr;
                static std::map<void *, std::vector<JSValue>> s_states;

                struct EffectSlot
                {
                    JSValue callback = JS_UNDEFINED;
                    JSValue cleanup = JS_UNDEFINED;
                    std::vector<JSValue> deps;
                    bool hasRun = false;
                };
                static std::map<void *, std::vector<EffectSlot>> s_effects;
                static std::vector<std::pair<void *, int>> s_pendingEffects;

                static void *s_active_comp;
                static int s_hook_idx;
                static int s_effect_idx;
                static JSValue s_root_element;
            };

            void drawNode(Bokken::Renderer::SpriteBatcher &batcher,
                           std::shared_ptr<Bokken::Canvas::Node> node,
                           int layer);
        }
    }
}