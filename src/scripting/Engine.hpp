#pragma once

#include <quickjs.h>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include "./modules/Base.hpp"
#include "../AssetPack.hpp"

namespace Bokken
{
    namespace Scripting
    {

        /**
         * Owns and manages the QuickJS runtime and context for one game session.
         *
         * Responsibilities:
         *   - Create / configure the JSRuntime and JSContext from ProjectConfiguration values.
         *   - Hold all registered BokkenModules and install them before script evaluation.
         *   - Load compiled bytecode (.script files produced by BokkenPacker).
         *   - Expose callHook() so the EngineLoop can drive onStart / onUpdate / onFixedUpdate.
         *   - Drain the QuickJS job queue (Promise microtasks) after each hook invocation.
         *   - Provide a structured shutdown path that calls destroy() on every module.
         */
        class Engine
        {
        public:
            Engine() = default;
            ~Engine() { shutdown(); }

            // Non-copyable.
            Engine(const Engine &) = delete;
            Engine &operator=(const Engine &) = delete;

            // Setup (call in this order before the game loop starts)

            /**
             * Initialises the JSRuntime and JSContext.
             * @param maxHeapMb  Maximum heap in megabytes (from Engine::Runtime config).
             * @param stackKb    Stack size in kilobytes.
             * @param gcThreshKb GC trigger threshold in kilobytes.
             * @return true on success.
             */
            bool init(AssetPack* assets, int maxHeapMb = 128, int stackKb = 1024, int gcThreshKb = 512);

            /**
             * Register a native module. Must be called after init() and before loadBytecode().
             * Modules are installed in registration order.
             */
            void addModule(std::unique_ptr<Modules::Base> module);

            /**
             * Load and evaluate a compiled .script bytecode blob.
             * The module's exported onStart/onUpdate/onFixedUpdate functions are cached
             * from the module namespace after evaluation.
             * @param data  Raw bytecode bytes.
             * @param len   Byte count.
             * @param name  Logical name used in error messages (e.g. "scripts/index.script").
             * @return true if the bytecode was loaded and evaluated without error.
             */
            bool loadBytecode(const uint8_t *data, size_t len, const std::string &name);

            // Print and clear any pending JS exception.
            void reportException(const std::string &context);

            // Game-loop interface

            /** Call the JS onStart() export once at the start of the session. */
            void callOnStart();

            /** Call the JS onUpdate(deltaTime) export every frame. */
            void callOnUpdate(double deltaTime);

            /** Call the JS onFixedUpdate(deltaTime) export at the fixed timestep. */
            void callOnFixedUpdate(double deltaTime);

            // Shutdown

            /** Free all JS values, call destroy() on modules, free context and runtime. */
            void shutdown();

            // Accessors (for modules that need to register additional JS objects)
            JSRuntime *runtime() const { return m_rt; }
            JSContext *context() const { return m_ctx; }

            /** Returns true if the engine has been successfully initialised. */
            bool isReady() const { return m_rt != nullptr && m_ctx != nullptr; }

        private:
            JSRuntime *m_rt = nullptr;
            JSContext *m_ctx = nullptr;

            AssetPack* m_assets = nullptr; // Store the VFS reference for module loading

            std::vector<std::unique_ptr<Modules::Base>> m_modules;

            // Cached references to the JS hook functions.
            JSValue m_fn_onStart = JS_UNDEFINED;
            JSValue m_fn_onUpdate = JS_UNDEFINED;
            JSValue m_fn_onFixedUpdate = JS_UNDEFINED;

            // Stub module loader used during bytecode evaluation so that imports of
            // "bokken/*" don't error out while we're resolving the top-level module.
            static JSModuleDef *s_module_loader(JSContext *ctx, const char *name, void *opaque);

            // Drain Promise microtasks / pending jobs after each hook call.
            void drainJobQueue();

            // Helper: call a cached JS function with zero or one double argument.
            void callCachedFn(JSValue fn, const std::string &hookName,
                              double *arg = nullptr);

            // Extract the lifecycle hooks from the module namespace after evaluation.
            void extractLifecycle(const std::string &modulePath);
        };
    } // namespace Scripting

} // namespace Bokken
