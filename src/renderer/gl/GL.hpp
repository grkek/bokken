#pragma once

/*
 * OpenGL 3.3 Core surface — backed by glad 2.
 *
 * Switched from GLEW to glad after GLEW's CMake build failed to link on
 * modern macOS SDKs (it appended `-framework AGL`, which Apple removed
 * around macOS 14). glad has none of that baggage:
 *
 *   - It's a code generator, not a runtime DLL. CMake invokes it at
 *     configure time and adds the generated `glad/gl.{h,c}` to our build.
 *   - Loading goes through whatever loader function we hand it; we pass
 *     `SDL_GL_GetProcAddress` and don't rely on any platform-specific
 *     behaviour (wglGetProcAddress, glXGetProcAddress, NSGLGetProcAddress).
 *   - No transitive link dependencies. The generated code references
 *     only its own static function-pointer table.
 *
 * Including this header is sufficient to use any GL symbol — `<glad/gl.h>`
 * brings in everything. There is no longer a `Bokken::Renderer::GL`
 * namespace re-exporting individual functions; that wrapper existed only
 * because the very first version of this engine hand-loaded ~30 entry
 * points via `SDL_GL_GetProcAddress`.
 *
 * Notes:
 *   - glad must be initialised AFTER an SDL GL context is current.
 *     See Renderer::Base::init for the canonical sequence.
 *   - Order of includes matters: include this header BEFORE <SDL3/SDL.h>
 *     in any TU that uses both. SDL3 may pull in system GL headers
 *     indirectly on some platforms, and glad's include guards expect to
 *     win that race.
 */

#include <glad/gl.h>

namespace Bokken
{
    namespace Renderer
    {
        namespace GL
        {

            /**
             * Initialise glad against the currently-current GL context.
             *
             * Must be called AFTER SDL_GL_CreateContext + SDL_GL_MakeCurrent,
             * and before any GL call. Returns false if the loader fails or
             * the context doesn't expose at least 3.3 core functionality.
             *
             * Signature preserved from the previous loader so callers
             * (Renderer::Base::init) don't change.
             */
            bool load();

            /**
             * Log GL_VENDOR / GL_RENDERER / GL_VERSION at startup. Useful
             * for triaging driver bugs and confirming the right context
             * was actually selected.
             */
            void logInfo();

            /**
             * Drain glGetError and log anything found. Useful after major
             * operations during development. Compile-out for release if it
             * shows up in profiles.
             */
            void checkError(const char *where);

        }
    }
} // namespace
