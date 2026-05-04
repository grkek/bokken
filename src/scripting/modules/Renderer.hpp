#pragma once

#include "Base.hpp"
#include "../../renderer/Base.hpp"
#include "../../renderer/Pipeline.hpp"
#include "../../renderer/stages/SpriteStage.hpp"
#include "../../renderer/stages/BloomStage.hpp"
#include "../../renderer/stages/ColorGradeStage.hpp"
#include "../../renderer/stages/CompositeStage.hpp"
#include "../../renderer/stages/DistortionStage.hpp"

#include <SDL3/SDL.h>

#include <string>
#include <memory>

namespace Bokken
{
    namespace Scripting
    {
        namespace Modules
        {

            /**
             * `bokken/renderer` — JS-facing pipeline configuration.
             *
             * Exports a singleton `pipeline` object with:
             *   pipeline.addStage(kindString, name, props?)   // built-in stages
             *   pipeline.removeStage(name)
             *   pipeline.moveStage(name, index)
             *   pipeline.setEnabled(name, bool)
             *   pipeline.configure(name, props)               // tunables
             *   pipeline.list()                                // names in order
             *
             * Built-in stage kinds: "sprite", "bloom", "color-grade", "composite".
             *
             * Wires the engine renderer into Canvas (sprite batcher) and Label
             * (glyph cache) at module-init time, so the JS user doesn't have
             * to think about plumbing.
             *
             * Example:
             *   import Renderer from "bokken/renderer";
             *   Renderer.pipeline.addStage("bloom", "bloom",
             *       { threshold: 0.7, intensity: 0.5 });
             *   Renderer.pipeline.moveStage("bloom", 1);   // after scene
             *   Renderer.pipeline.configure("color-grade", { exposure: 1.2 });
             */
            class Renderer : public Base
            {
            public:
                Renderer(Bokken::Renderer::Base *renderer, Bokken::AssetPack *assets = nullptr)
                    : Base("bokken/renderer")
                {
                    s_renderer = renderer;
                    s_assets = assets;
                }

                int declare(JSContext *ctx, JSModuleDef *m) override;
                int init(JSContext *ctx, JSModuleDef *m) override;

            private:
                static inline Bokken::Renderer::Base *s_renderer = nullptr;
                static inline Bokken::AssetPack *s_assets = nullptr;

                // Pipeline functions.
                static JSValue js_pipeline_add_stage(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_pipeline_remove_stage(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_pipeline_move_stage(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_pipeline_set_enabled(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_pipeline_configure(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_pipeline_list(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

                // Texture functions.
                static JSValue js_load_texture(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_define_region(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_define_grid(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

                // Distortion functions.
                static JSValue js_add_shockwave(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_clear_shockwaves(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
            };

        }
    }
}