#include "Engine.hpp"
#include <cstdio>
#include <cstring>

namespace Bokken
{
    namespace Scripting
    {
        // Static module loader callback for QuickJS-NG. This is registered as a fallback
        // for any import path that doesn't match a registered Modules::Base. It attempts
        // to load modules from the AssetPack, allowing users to import bytecode modules
        // directly from their scripts.
        JSModuleDef *Engine::s_module_loader(JSContext *ctx, const char *name, void *opaque)
        {
            // Cast opaque back to our Scripting Engine
            auto *engine = static_cast<Engine *>(opaque);

            // 1. Check native modules
            if (strncmp(name, "bokken/", 7) == 0)
            {
                return nullptr;
            }

            // 2. Load from AssetPack
            if (engine && engine->m_assets && engine->m_assets->exists(name))
            {
                std::vector<uint8_t> bc = engine->m_assets->readBytes(name);
                if (!bc.empty())
                {
                    JSValue obj = JS_ReadObject(ctx, bc.data(), bc.size(), JS_READ_OBJ_BYTECODE);
                    if (!JS_IsException(obj))
                    {
                        JSModuleDef *mod = (JSModuleDef *)JS_VALUE_GET_PTR(obj);
                        JS_FreeValue(ctx, obj);
                        return mod;
                    }
                    JS_FreeValue(ctx, obj);
                }
            }

            fprintf(stderr, "[Bokken] Module not found: %s\n", name);
            return nullptr;
        }

        // Lifecycle
        bool Engine::init(AssetPack *assets, int maxHeapMb, int stackKb, int gcThreshKb)
        {
            m_assets = assets;

            m_rt = JS_NewRuntime();
            if (!m_rt)
            {
                fprintf(stderr, "[Bokken] Failed to create JSRuntime\n");
                return false;
            }

            // Apply memory / stack limits from the project configuration.
            JS_SetMemoryLimit(m_rt, static_cast<size_t>(maxHeapMb) * 1024 * 1024);
            JS_SetMaxStackSize(m_rt, static_cast<size_t>(stackKb) * 1024);
            JS_SetGCThreshold(m_rt, static_cast<size_t>(gcThreshKb) * 1024);

            m_ctx = JS_NewContext(m_rt);
            if (!m_ctx)
            {
                fprintf(stderr, "[Bokken] Failed to create JSContext\n");
                JS_FreeRuntime(m_rt);
                m_rt = nullptr;
                return false;
            }

            // Install the stub module loader. It acts as a fallback for any import
            // path that doesn't match a registered Modules::Base.
            JS_SetModuleLoaderFunc(m_rt, nullptr, &Engine::s_module_loader, this);

            return true;
        }

        void Engine::addModule(std::unique_ptr<Modules::Base> module)
        {
            if (!m_ctx)
            {
                fprintf(stderr, "[Bokken] addModule() called before init()\n");
                return;
            }
            module->registerInto(m_ctx);
            m_modules.push_back(std::move(module));
        }

        bool Engine::loadBytecode(const uint8_t *data, size_t len, const std::string &name)
        {
            if (!m_ctx)
                return false;

            JSValue object = JS_ReadObject(m_ctx, data, len, JS_READ_OBJ_BYTECODE);
            if (JS_IsException(object))
            {
                fprintf(stderr, "[Bokken] Critical: Failed to parse bytecode for '%s'\n", name.c_str());
                reportException("ReadObject: " + name);
                return false;
            }

            if (JS_ResolveModule(m_ctx, object) != 0)
            {
                fprintf(stderr, "[Bokken] Critical: Failed to resolve module '%s'\n", name.c_str());
                reportException("ResolveModule: " + name);
                JS_FreeValue(m_ctx, object);
                return false;
            }

            JSValue result = JS_EvalFunction(m_ctx, object);
            if (JS_IsException(result))
            {
                fprintf(stderr, "[Bokken] Critical: Module Evaluation/Linking failed for '%s'\n", name.c_str());
                reportException("EvalFunction: " + name);
                JS_FreeValue(m_ctx, result);
                return false;
            }
            JS_FreeValue(m_ctx, result);

            extractLifecycle(name);
            return true;
        }

        void Engine::shutdown()
        {
            if (!m_rt)
                return;

            // We move the pointers to local variables and set the class members to null.
            // Now, any other thread or any lambda calling isReady() will get 'false' instantly.
            JSContext *ctx = m_ctx;
            JSRuntime *rt = m_rt;

            m_ctx = nullptr;
            m_rt = nullptr;

            if (ctx)
            {
                // Free cached hook references
                JS_FreeValue(ctx, m_fn_onStart);
                JS_FreeValue(ctx, m_fn_onUpdate);
                JS_FreeValue(ctx, m_fn_onFixedUpdate);

                m_fn_onStart = JS_UNDEFINED;
                m_fn_onUpdate = JS_UNDEFINED;
                m_fn_onFixedUpdate = JS_UNDEFINED;

                // Let each module free its own native resources.
                for (auto &mod : m_modules)
                {
                    mod->destroy();
                }
                
                m_modules.clear();

                JS_FreeContext(ctx);
            }

            if (rt)
            {
                Modules::Base::unregisterRuntime(rt);
                JS_FreeRuntime(rt);
            }
        }

        // Game-loop hooks
        void Engine::callOnStart()
        {
            callCachedFn(m_fn_onStart, "onStart");
        }

        void Engine::callOnUpdate(double deltaTime)
        {
            callCachedFn(m_fn_onUpdate, "onUpdate", &deltaTime);
        }

        void Engine::callOnFixedUpdate(double deltaTime)
        {
            callCachedFn(m_fn_onFixedUpdate, "onFixedUpdate", &deltaTime);
        }

        // Private helpers
        void Engine::callCachedFn(JSValue fn, const std::string &hookName,
                                  double *arg)
        {
            if (!m_ctx || JS_IsUndefined(fn))
                return;

            JSValue result;
            if (arg)
            {
                JSValue jsArg = JS_NewFloat64(m_ctx, *arg);
                result = JS_Call(m_ctx, fn, JS_UNDEFINED, 1, &jsArg);
                JS_FreeValue(m_ctx, jsArg);
            }
            else
            {
                result = JS_Call(m_ctx, fn, JS_UNDEFINED, 0, nullptr);
            }

            if (JS_IsException(result))
            {
                reportException(hookName);
            }
            JS_FreeValue(m_ctx, result);

            // Always drain microtasks after a hook call so that async/await in user
            // scripts progresses without needing an explicit flush call.
            drainJobQueue();
        }

        void Engine::drainJobQueue()
        {
            JSContext *pctx = nullptr;
            int ret;
            while ((ret = JS_ExecutePendingJob(m_rt, &pctx)) > 0)
            {
                // Keep draining.
            }
            if (ret < 0 && pctx)
            {
                reportException("drainJobQueue");
            }
        }

        void Engine::reportException(const std::string &context)
        {
            if (!m_ctx)
                return;

            JSValue exc = JS_GetException(m_ctx);

            // 1. Get the primary message
            JSValue str = JS_ToString(m_ctx, exc);
            const char *msg = JS_ToCString(m_ctx, str);

            fprintf(stderr, "Context: %s\n", context.c_str());
            fprintf(stderr, "Message: %s\n", msg ? msg : "<unknown>");

            JS_FreeCString(m_ctx, msg);
            JS_FreeValue(m_ctx, str);

            // 2. Extract deep metadata if it's an Error object
            if (JS_IsError(exc))
            {
                // Get File and Line info
                auto logExtra = [&](const char *prop, const char *label)
                {
                    JSValue val = JS_GetPropertyStr(m_ctx, exc, prop);
                    if (!JS_IsUndefined(val))
                    {
                        const char *cval = JS_ToCString(m_ctx, val);
                        if (cval)
                        {
                            fprintf(stderr, "%s: %s\n", label, cval);
                            JS_FreeCString(m_ctx, cval);
                        }
                    }
                    JS_FreeValue(m_ctx, val);
                };

                logExtra("fileName", "File");
                logExtra("lineNumber", "Line");

                // Get Stack Trace
                JSValue stack = JS_GetPropertyStr(m_ctx, exc, "stack");
                if (!JS_IsUndefined(stack))
                {
                    const char *stackStr = JS_ToCString(m_ctx, stack);
                    if (stackStr)
                    {
                        std::string s = stackStr;
                        size_t pos = 0;
                        // CORRECTED: Use std::string::npos
                        while ((pos = s.find('\n', pos)) != std::string::npos)
                        {
                            s.replace(pos, 1, "\n    ");
                            pos += 5; // move past the newline and indentation
                        }
                        fprintf(stderr, "Stack Trace:\n    %s\n", s.c_str());
                        JS_FreeCString(m_ctx, stackStr);
                    }
                }
                JS_FreeValue(m_ctx, stack);
            }
            fprintf(stderr, "-----------------------------\n\n");

            JS_FreeValue(m_ctx, exc);
        }

        void Engine::extractLifecycle(const std::string &modulePath)
        {
            // Bridge script to move exports to globalThis
            std::string bridgeScript =
                "import('" + modulePath + "').then(m => {"
                                          "  globalThis.__onStart = m.onStart;"
                                          "  globalThis.__onUpdate = m.onUpdate;"
                                          "  globalThis.__onFixedUpdate = m.onFixedUpdate;"
                                          "}).catch(e => console.error('[Bokken] Extract Error:', e));";

            JSValue val = JS_Eval(m_ctx, bridgeScript.c_str(), bridgeScript.size(),
                                  "<lifecycle_bridge>", JS_EVAL_TYPE_GLOBAL);

            if (JS_IsException(val))
            {
                reportException("LifecycleBridge");
            }
            JS_FreeValue(m_ctx, val);

            // Execute the .then() callback
            drainJobQueue();

            JSValue global = JS_GetGlobalObject(m_ctx);

            auto syncHook = [&](const char *prop, JSValue &member)
            {
                JS_FreeValue(m_ctx, member); // Prevent leaks on reload

                JSValue fn = JS_GetPropertyStr(m_ctx, global, prop);
                if (JS_IsFunction(m_ctx, fn))
                {
                    member = fn;
                }
                else
                {
                    member = JS_UNDEFINED;
                    JS_FreeValue(m_ctx, fn);
                }
                // Clean up global temp property
                JS_SetPropertyStr(m_ctx, global, prop, JS_UNDEFINED);
            };

            syncHook("__onStart", m_fn_onStart);
            syncHook("__onUpdate", m_fn_onUpdate);
            syncHook("__onFixedUpdate", m_fn_onFixedUpdate);

            JS_FreeValue(m_ctx, global);
        }
    } // namespace Scripting

} // namespace Bokken
