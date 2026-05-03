#include "SpriteStage.hpp"

namespace Bokken
{
    namespace Renderer
    {

        void SpriteStage::execute(const FrameContext &ctx)
        {
            if (!ctx.outputTarget || !ctx.batcher)
                return;

            ctx.outputTarget->bind();
            glViewport(0, 0, ctx.viewportWidth, ctx.viewportHeight);
            glClearColor(clearR, clearG, clearB, clearA);
            glClear(GL_COLOR_BUFFER_BIT);

            // Batcher's begin() is called once per frame by Renderer (so user
            // code can submit between frames). We just flush here.
            ctx.batcher->flush();
        }

    }
}
