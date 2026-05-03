#include "GL.hpp"

#include <SDL3/SDL.h>
#include <cstdio>

namespace Bokken
{
    namespace Renderer
    {
        namespace GL
        {

            bool load()
            {
                // glad's loader takes a callback with signature
                //   void* (const char *)
                // SDL3's SDL_GL_GetProcAddress returns an SDL_FunctionPointer
                // (a typed function-pointer alias), not void*. They're
                // ABI-compatible — SDL_FunctionPointer is just a typed
                // function pointer — but the C++ type system needs the
                // explicit reinterpret_cast.
                const int version = gladLoadGL(
                    reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress));

                if (version == 0)
                {
                    SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                                 "[GL] gladLoadGL failed — no GL context current?");
                    return false;
                }

                const int major = GLAD_VERSION_MAJOR(version);
                const int minor = GLAD_VERSION_MINOR(version);
                if (major < 3 || (major == 3 && minor < 3))
                {
                    SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                                 "[GL] need GL 3.3 core, got %d.%d", major, minor);
                    return false;
                }

                return true;
            }

            void logInfo()
            {
                const auto *vendor   = glGetString(GL_VENDOR);
                const auto *renderer = glGetString(GL_RENDERER);
                const auto *version  = glGetString(GL_VERSION);
                SDL_LogInfo(SDL_LOG_CATEGORY_RENDER, "[GL] vendor:   %s",
                            vendor   ? reinterpret_cast<const char *>(vendor)   : "?");
                SDL_LogInfo(SDL_LOG_CATEGORY_RENDER, "[GL] renderer: %s",
                            renderer ? reinterpret_cast<const char *>(renderer) : "?");
                SDL_LogInfo(SDL_LOG_CATEGORY_RENDER, "[GL] version:  %s",
                            version  ? reinterpret_cast<const char *>(version)  : "?");
            }

            void checkError(const char *where)
            {
                // Bound the loop in case the driver is reporting errors
                // every call (some debug drivers do).
                for (int i = 0; i < 8; i++)
                {
                    const GLenum err = glGetError();
                    if (err == GL_NO_ERROR)
                        return;
                    const char *name = "?";
                    switch (err)
                    {
                    case GL_INVALID_ENUM:                  name = "GL_INVALID_ENUM"; break;
                    case GL_INVALID_VALUE:                 name = "GL_INVALID_VALUE"; break;
                    case GL_INVALID_OPERATION:             name = "GL_INVALID_OPERATION"; break;
                    case GL_INVALID_FRAMEBUFFER_OPERATION: name = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
                    case GL_OUT_OF_MEMORY:                 name = "GL_OUT_OF_MEMORY"; break;
                    default: break;
                    }
                    SDL_LogError(SDL_LOG_CATEGORY_RENDER, "[GL] %s: 0x%x (%s)",
                                 where ? where : "?", err, name);
                }
            }

        }
    }
}
