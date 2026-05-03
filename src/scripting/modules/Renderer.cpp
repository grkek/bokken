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
                JS_SetModuleExport(ctx, m, "default", def);
                return 0;
            }

        }
    }
}
