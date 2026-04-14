#include "Base.hpp"
#include <SDL3/SDL.h>

namespace Bokken
{
    namespace Modules
    {
        class Window : public Base
        {
        private:
            // Standard QuickJS C-functions can't capture 'this',
            // so we use a static pointer for the module instance.
            static inline SDL_Window *s_window = nullptr;

        public:
            Window(SDL_Window *window)
                : Base("bokken/window")
            {
                s_window = window;
            }

            int declare(JSContext *ctx, JSModuleDef *m) override
            {
                return JS_AddModuleExport(ctx, m, "default");
            }

            int init(JSContext *ctx, JSModuleDef *m) override
            {
                JSValue defaultExport = JS_NewObject(ctx);

                // setTitle lambda
                auto setTitle = [](JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) -> JSValue
                {
                    if (argc >= 1 && s_window)
                    {
                        const char *title = JS_ToCString(ctx, argv[0]);

                        SDL_SetWindowTitle(s_window, title);
                        JS_FreeCString(ctx, title);
                    }
                    return JS_UNDEFINED;
                };

                // getSize lambda
                auto getSize = [](JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) -> JSValue
                {
                    int w = 0, h = 0;

                    if (s_window)
                    {
                        SDL_GetWindowSize(s_window, &w, &h);
                    }

                    JSValue obj = JS_NewObject(ctx);

                    JS_SetPropertyStr(ctx, obj, "width", JS_NewInt32(ctx, w));
                    JS_SetPropertyStr(ctx, obj, "height", JS_NewInt32(ctx, h));

                    return obj;
                };

                // Attach functions just like your Log module
                JS_SetPropertyStr(ctx, defaultExport, "setTitle",
                                  JS_NewCFunction(ctx, setTitle, "setTitle", 1));

                JS_SetPropertyStr(ctx, defaultExport, "getSize",
                                  JS_NewCFunction(ctx, getSize, "getSize", 0));

                JS_SetModuleExport(ctx, m, "default", defaultExport);
                return 0;
            }
        };

    } // namespace Modules
} // namespace Bokken