#include "Renderer.hpp"

namespace Bokken
{
    namespace Scripting
    {
        namespace Modules
        {
            namespace
            {
                // Read a numeric property off a JS object, leaving `out` unchanged
                // if the prop is missing/non-numeric. Returns true if it wrote.
                bool readNumber(JSContext *ctx, JSValueConst obj, const char *name, float &out)
                {
                    if (!JS_IsObject(obj))
                        return false;
                    JSValue v = JS_GetPropertyStr(ctx, obj, name);
                    bool got = false;
                    if (JS_IsNumber(v))
                    {
                        double d = 0;
                        if (JS_ToFloat64(ctx, &d, v) == 0)
                        {
                            out = (float)d;
                            got = true;
                        }
                    }
                    JS_FreeValue(ctx, v);
                    return got;
                }

                bool readBool(JSContext *ctx, JSValueConst obj, const char *name, bool &out)
                {
                    if (!JS_IsObject(obj))
                        return false;
                    JSValue v = JS_GetPropertyStr(ctx, obj, name);
                    bool got = false;
                    if (JS_IsBool(v))
                    {
                        out = (JS_ToBool(ctx, v) != 0);
                        got = true;
                    }
                    JS_FreeValue(ctx, v);
                    return got;
                }

                // Apply property bag to a known stage by walking its tunables.
                // Unknown properties are silently ignored — stage shape is the
                // contract the JS user reads from the docs.
                void applyProps(JSContext *ctx, Bokken::Renderer::Stage *st, JSValueConst props)
                {
                    if (!st || !JS_IsObject(props))
                        return;

                    bool en = st->enabled;
                    if (readBool(ctx, props, "enabled", en))
                        st->enabled = en;

                    // Stage-specific fields. We use dynamic_cast — these are only
                    // a handful of types and the JS-facing layer is the one place
                    // where dispatch by concrete type is acceptable.
                    if (auto *bs = dynamic_cast<Bokken::Renderer::BloomStage *>(st))
                    {
                        readNumber(ctx, props, "threshold", bs->threshold);
                        readNumber(ctx, props, "intensity", bs->intensity);
                        readNumber(ctx, props, "radius", bs->radius);
                    }
                    if (auto *cg = dynamic_cast<Bokken::Renderer::ColorGradeStage *>(st))
                    {
                        readNumber(ctx, props, "exposure", cg->exposure);
                        readNumber(ctx, props, "saturation", cg->saturation);
                        readNumber(ctx, props, "gamma", cg->gamma);
                    }
                    if (auto *ss = dynamic_cast<Bokken::Renderer::SpriteStage *>(st))
                    {
                        readNumber(ctx, props, "clearR", ss->clearR);
                        readNumber(ctx, props, "clearG", ss->clearG);
                        readNumber(ctx, props, "clearB", ss->clearB);
                        readNumber(ctx, props, "clearA", ss->clearA);
                    }
                    if (auto *ds = dynamic_cast<Bokken::Renderer::DistortionStage *>(st))
                    {
                        readNumber(ctx, props, "intensity", ds->intensity);
                        readNumber(ctx, props, "heatHazeSpeed", ds->heatHazeSpeed);
                        readNumber(ctx, props, "heatHazeFrequency", ds->heatHazeFrequency);
                        readNumber(ctx, props, "heatHazeAmplitude", ds->heatHazeAmplitude);

                        bool haze = ds->heatHaze;
                        if (readBool(ctx, props, "heatHaze", haze))
                            ds->heatHaze = haze;
                    }
                }

                // Build a stage of a given kind. Returns nullptr if kind unknown.
                std::unique_ptr<Bokken::Renderer::Stage> makeStage(const std::string &kind, const std::string &name)
                {
                    if (kind == "sprite")
                        return std::make_unique<Bokken::Renderer::SpriteStage>(name);
                    if (kind == "bloom")
                        return std::make_unique<Bokken::Renderer::BloomStage>(name);
                    if (kind == "color-grade")
                        return std::make_unique<Bokken::Renderer::ColorGradeStage>(name);
                    if (kind == "distortion")
                        return std::make_unique<Bokken::Renderer::DistortionStage>(name);
                    if (kind == "composite")
                        return std::make_unique<Bokken::Renderer::CompositeStage>(name);
                    return nullptr;
                }
            } // namespace

            // ── JS-facing functions ──

            JSValue Renderer::js_pipeline_add_stage(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
            {
                if (!s_renderer)
                    return JS_FALSE;
                if (argc < 2)
                {
                    return JS_ThrowTypeError(ctx, "pipeline.addStage(kind, name, props?) requires (string, string)");
                }
                const char *kindC = JS_ToCString(ctx, argv[0]);
                const char *nameC = JS_ToCString(ctx, argv[1]);
                if (!kindC || !nameC)
                {
                    if (kindC)
                        JS_FreeCString(ctx, kindC);
                    if (nameC)
                        JS_FreeCString(ctx, nameC);
                    return JS_FALSE;
                }
                std::string kind = kindC, name = nameC;
                JS_FreeCString(ctx, kindC);
                JS_FreeCString(ctx, nameC);

                auto stage = makeStage(kind, name);
                if (!stage)
                {
                    return JS_ThrowTypeError(ctx, "Unknown stage kind '%s'", kind.c_str());
                }
                // Apply props before handing off — addStage runs setup() and resize().
                if (argc >= 3)
                    applyProps(ctx, stage.get(), argv[2]);

                s_renderer->pipeline().addStage(std::move(stage));
                return JS_TRUE;
            }

            JSValue Renderer::js_pipeline_remove_stage(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
            {
                if (!s_renderer || argc < 1)
                    return JS_FALSE;
                const char *nameC = JS_ToCString(ctx, argv[0]);
                if (!nameC)
                    return JS_FALSE;
                bool ok = s_renderer->pipeline().removeStage(nameC);
                JS_FreeCString(ctx, nameC);
                return ok ? JS_TRUE : JS_FALSE;
            }

            JSValue Renderer::js_pipeline_move_stage(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
            {
                if (!s_renderer || argc < 2)
                    return JS_FALSE;
                const char *nameC = JS_ToCString(ctx, argv[0]);
                if (!nameC)
                    return JS_FALSE;
                int32_t idx = 0;
                JS_ToInt32(ctx, &idx, argv[1]);
                bool ok = s_renderer->pipeline().moveStage(nameC, idx);
                JS_FreeCString(ctx, nameC);
                return ok ? JS_TRUE : JS_FALSE;
            }

            JSValue Renderer::js_pipeline_set_enabled(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
            {
                if (!s_renderer || argc < 2)
                    return JS_FALSE;
                const char *nameC = JS_ToCString(ctx, argv[0]);
                if (!nameC)
                    return JS_FALSE;
                Bokken::Renderer::Stage *st = s_renderer->pipeline().findStage(nameC);
                JS_FreeCString(ctx, nameC);
                if (!st)
                    return JS_FALSE;
                st->enabled = (JS_ToBool(ctx, argv[1]) != 0);
                return JS_TRUE;
            }

            JSValue Renderer::js_pipeline_configure(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
            {
                if (!s_renderer || argc < 2)
                    return JS_FALSE;
                const char *nameC = JS_ToCString(ctx, argv[0]);
                if (!nameC)
                    return JS_FALSE;
                Bokken::Renderer::Stage *st = s_renderer->pipeline().findStage(nameC);
                JS_FreeCString(ctx, nameC);
                if (!st)
                    return JS_FALSE;
                applyProps(ctx, st, argv[1]);
                return JS_TRUE;
            }

            JSValue Renderer::js_pipeline_list(JSContext *ctx, JSValueConst, int /*argc*/, JSValueConst * /*argv*/)
            {
                if (!s_renderer)
                    return JS_NewArray(ctx);
                const auto &stages = s_renderer->pipeline().stages();
                JSValue arr = JS_NewArray(ctx);
                for (size_t i = 0; i < stages.size(); ++i)
                {
                    JS_SetPropertyUint32(ctx, arr, (uint32_t)i,
                                         JS_NewString(ctx, stages[i]->name().c_str()));
                }
                return arr;
            }

            // JS: Renderer.loadTexture(path, filter?)
            //     filter: "linear" (default) or "nearest"
            //     Returns true on success.
            JSValue Renderer::js_load_texture(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
            {
                if (!s_renderer || !s_assets || argc < 1)
                    return JS_FALSE;

                const char *path = JS_ToCString(ctx, argv[0]);
                if (!path)
                    return JS_FALSE;

                Bokken::Renderer::TextureFilter filter = Bokken::Renderer::TextureFilter::Linear;
                if (argc >= 2)
                {
                    const char *filterStr = JS_ToCString(ctx, argv[1]);
                    if (filterStr)
                    {
                        if (strcmp(filterStr, "nearest") == 0)
                            filter = Bokken::Renderer::TextureFilter::Nearest;
                        JS_FreeCString(ctx, filterStr);
                    }
                }

                const Bokken::Renderer::Texture2D *tex =
                    s_renderer->textures().load(path, s_assets, filter);
                JS_FreeCString(ctx, path);
                return tex ? JS_TRUE : JS_FALSE;
            }

            // JS: Renderer.defineRegion(name, texturePath, x, y, w, h)
            JSValue Renderer::js_define_region(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
            {
                if (!s_renderer || argc < 6)
                    return JS_FALSE;

                const char *name = JS_ToCString(ctx, argv[0]);
                const char *texPath = JS_ToCString(ctx, argv[1]);
                if (!name || !texPath)
                {
                    if (name) JS_FreeCString(ctx, name);
                    if (texPath) JS_FreeCString(ctx, texPath);
                    return JS_FALSE;
                }

                int32_t x, y, w, h;
                JS_ToInt32(ctx, &x, argv[2]);
                JS_ToInt32(ctx, &y, argv[3]);
                JS_ToInt32(ctx, &w, argv[4]);
                JS_ToInt32(ctx, &h, argv[5]);

                s_renderer->textures().defineRegion(name, texPath, x, y, w, h);
                JS_FreeCString(ctx, name);
                JS_FreeCString(ctx, texPath);
                return JS_TRUE;
            }

            // JS: Renderer.defineGrid(prefix, texturePath, frameW, frameH, props?)
            //     props: { count?, offsetX?, offsetY?, paddingX?, paddingY? }
            //     Returns the number of regions created.
            JSValue Renderer::js_define_grid(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
            {
                if (!s_renderer || argc < 4)
                    return JS_NewInt32(ctx, 0);

                const char *prefix = JS_ToCString(ctx, argv[0]);
                const char *texPath = JS_ToCString(ctx, argv[1]);
                if (!prefix || !texPath)
                {
                    if (prefix) JS_FreeCString(ctx, prefix);
                    if (texPath) JS_FreeCString(ctx, texPath);
                    return JS_NewInt32(ctx, 0);
                }

                int32_t frameW, frameH;
                JS_ToInt32(ctx, &frameW, argv[2]);
                JS_ToInt32(ctx, &frameH, argv[3]);

                int count = 0, offX = 0, offY = 0, padX = 0, padY = 0;

                if (argc >= 5 && JS_IsObject(argv[4]))
                {
                    JSValue v;
                    int32_t tmp;

                    v = JS_GetPropertyStr(ctx, argv[4], "count");
                    if (JS_IsNumber(v)) { JS_ToInt32(ctx, &tmp, v); count = tmp; }
                    JS_FreeValue(ctx, v);

                    v = JS_GetPropertyStr(ctx, argv[4], "offsetX");
                    if (JS_IsNumber(v)) { JS_ToInt32(ctx, &tmp, v); offX = tmp; }
                    JS_FreeValue(ctx, v);

                    v = JS_GetPropertyStr(ctx, argv[4], "offsetY");
                    if (JS_IsNumber(v)) { JS_ToInt32(ctx, &tmp, v); offY = tmp; }
                    JS_FreeValue(ctx, v);

                    v = JS_GetPropertyStr(ctx, argv[4], "paddingX");
                    if (JS_IsNumber(v)) { JS_ToInt32(ctx, &tmp, v); padX = tmp; }
                    JS_FreeValue(ctx, v);

                    v = JS_GetPropertyStr(ctx, argv[4], "paddingY");
                    if (JS_IsNumber(v)) { JS_ToInt32(ctx, &tmp, v); padY = tmp; }
                    JS_FreeValue(ctx, v);
                }

                int created = s_renderer->textures().defineGrid(
                    prefix, texPath, frameW, frameH, count, offX, offY, padX, padY);
                JS_FreeCString(ctx, prefix);
                JS_FreeCString(ctx, texPath);
                return JS_NewInt32(ctx, created);
            }

            // JS: Renderer.addShockwave(x, y, props?)
            //     x, y: normalised screen coordinates (0..1).
            //     props: { speed?, thickness?, amplitude?, maxRadius? }
            //
            // Finds the first DistortionStage in the pipeline and adds a
            // shockwave to it. If no distortion stage exists, does nothing.
            JSValue Renderer::js_add_shockwave(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
            {
                if (!s_renderer || argc < 2)
                    return JS_FALSE;

                double x = 0, y = 0;
                JS_ToFloat64(ctx, &x, argv[0]);
                JS_ToFloat64(ctx, &y, argv[1]);

                float speed = 0.8f, thickness = 0.1f, amplitude = 0.06f, maxRadius = 1.5f;

                if (argc >= 3 && JS_IsObject(argv[2]))
                {
                    auto readF = [&](const char *prop, float &out)
                    {
                        JSValue v = JS_GetPropertyStr(ctx, argv[2], prop);
                        if (JS_IsNumber(v))
                        {
                            double d = 0;
                            JS_ToFloat64(ctx, &d, v);
                            out = static_cast<float>(d);
                        }
                        JS_FreeValue(ctx, v);
                    };

                    readF("speed", speed);
                    readF("thickness", thickness);
                    readF("amplitude", amplitude);
                    readF("maxRadius", maxRadius);
                }

                // Find the distortion stage in the pipeline.
                for (auto &stage : s_renderer->pipeline().stages())
                {
                    auto *ds = dynamic_cast<Bokken::Renderer::DistortionStage *>(stage.get());
                    if (ds)
                    {
                        ds->addShockwave(static_cast<float>(x), static_cast<float>(y),
                                         speed, thickness, amplitude, maxRadius);
                        return JS_TRUE;
                    }
                }

                return JS_FALSE;
            }

            // JS: Renderer.clearShockwaves()
            JSValue Renderer::js_clear_shockwaves(JSContext *ctx, JSValueConst, int, JSValueConst *)
            {
                if (!s_renderer)
                    return JS_UNDEFINED;

                for (auto &stage : s_renderer->pipeline().stages())
                {
                    auto *ds = dynamic_cast<Bokken::Renderer::DistortionStage *>(stage.get());
                    if (ds)
                    {
                        ds->clearShockwaves();
                        break;
                    }
                }

                return JS_UNDEFINED;
            }

            int Renderer::declare(JSContext *ctx, JSModuleDef *m)
            {
                return JS_AddModuleExport(ctx, m, "default");
            }

            int Renderer::init(JSContext *ctx, JSModuleDef *m)
            {
                JSValue def = JS_NewObject(ctx);
                JSValue pipe = JS_NewObject(ctx);

                JS_SetPropertyStr(ctx, pipe, "addStage",
                                  JS_NewCFunction(ctx, &Renderer::js_pipeline_add_stage, "addStage", 3));
                JS_SetPropertyStr(ctx, pipe, "removeStage",
                                  JS_NewCFunction(ctx, &Renderer::js_pipeline_remove_stage, "removeStage", 1));
                JS_SetPropertyStr(ctx, pipe, "moveStage",
                                  JS_NewCFunction(ctx, &Renderer::js_pipeline_move_stage, "moveStage", 2));
                JS_SetPropertyStr(ctx, pipe, "setEnabled",
                                  JS_NewCFunction(ctx, &Renderer::js_pipeline_set_enabled, "setEnabled", 2));
                JS_SetPropertyStr(ctx, pipe, "configure",
                                  JS_NewCFunction(ctx, &Renderer::js_pipeline_configure, "configure", 2));
                JS_SetPropertyStr(ctx, pipe, "list",
                                  JS_NewCFunction(ctx, &Renderer::js_pipeline_list, "list", 0));

                JS_SetPropertyStr(ctx, def, "pipeline", pipe);

                // Texture management functions.
                JS_SetPropertyStr(ctx, def, "loadTexture",
                                  JS_NewCFunction(ctx, &Renderer::js_load_texture, "loadTexture", 2));
                JS_SetPropertyStr(ctx, def, "defineRegion",
                                  JS_NewCFunction(ctx, &Renderer::js_define_region, "defineRegion", 6));
                JS_SetPropertyStr(ctx, def, "defineGrid",
                                  JS_NewCFunction(ctx, &Renderer::js_define_grid, "defineGrid", 5));

                // Distortion functions.
                JS_SetPropertyStr(ctx, def, "addShockwave",
                                  JS_NewCFunction(ctx, &Renderer::js_add_shockwave, "addShockwave", 3));
                JS_SetPropertyStr(ctx, def, "clearShockwaves",
                                  JS_NewCFunction(ctx, &Renderer::js_clear_shockwaves, "clearShockwaves", 0));

                JS_SetModuleExport(ctx, m, "default", def);
                return 0;
            }

        }
    }
}