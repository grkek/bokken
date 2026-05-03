#pragma once

#include "../Engine.hpp"
#include "Base.hpp"


namespace Bokken
{
    namespace Scripting
    {
        namespace Modules
        {
            class Audio : public Base
            {
            public:
                Audio() : Base("bokken/audio") {}
                ~Audio() override = default;

                int declare(JSContext *ctx, JSModuleDef *m) override
                {
                    JS_AddModuleExport(ctx, m, "default");

                    JS_AddModuleExport(ctx, m, "Master");

                    return 0;
                }

                int init(JSContext *ctx, JSModuleDef *m) override
                {
                    JSValue defaultExport = JS_NewObject(ctx);

                    JS_SetModuleExport(ctx, m, "default", defaultExport);

                    JS_SetModuleExport(ctx, m, "Master", JS_NewString(ctx, "Master"));

                    return 0;
                }
            };
        }
    }
}