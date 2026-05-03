#include "Base.hpp"
#include <cstdio>

namespace Bokken
{
    namespace Scripting
    {
        namespace Modules
        {
            class Log : public Base
            {
            public:
                Log() : Base("bokken/log") {}

                int declare(JSContext *ctx, JSModuleDef *m) override
                {
                    return JS_AddModuleExport(ctx, m, "default");
                }

                int init(JSContext *ctx, JSModuleDef *m) override
                {
                    JSValue defaultExport = JS_NewObject(ctx);

                    auto make_log_fn = [](JSContext *ctx, JSValueConst, int argc,
                                          JSValueConst *argv, int magic) -> JSValue
                    {
                        const char *message = JS_ToCString(ctx, argv[0]);

                        switch (magic)
                        {
                        case 0:
                            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
                            break;
                        case 1:
                            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
                            break;
                        case 2:
                            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
                            break;
                        case 3:
                            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
                            break;
                        }

                        JS_FreeCString(ctx, message);
                        return JS_UNDEFINED;
                    };

                    JS_SetPropertyStr(ctx, defaultExport, "debug",
                                      JS_NewCFunctionMagic(ctx, make_log_fn, "debug", 1, JS_CFUNC_generic_magic, 0));
                    JS_SetPropertyStr(ctx, defaultExport, "info",
                                      JS_NewCFunctionMagic(ctx, make_log_fn, "info", 1, JS_CFUNC_generic_magic, 1));
                    JS_SetPropertyStr(ctx, defaultExport, "warning",
                                      JS_NewCFunctionMagic(ctx, make_log_fn, "warning", 1, JS_CFUNC_generic_magic, 2));
                    JS_SetPropertyStr(ctx, defaultExport, "error",
                                      JS_NewCFunctionMagic(ctx, make_log_fn, "error", 1, JS_CFUNC_generic_magic, 3));

                    JS_SetModuleExport(ctx, m, "default", defaultExport);

                    return 0;
                }
            };

        } // namespace Modules
    } // namespace Scripting
} // namespace Bokken