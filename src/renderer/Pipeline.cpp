#include "Pipeline.hpp"

namespace Bokken {
namespace Renderer {

    bool Pipeline::init(int width, int height)
    {
        m_width = width;
        m_height = height;

        // Both targets are HDR so bloom / tonemap have headroom.
        // Depth is off — we don't need it for 2D.
        if (!m_targetA.create(width, height, TextureFormat::RGBA16F, false)) return false;
        if (!m_targetB.create(width, height, TextureFormat::RGBA16F, false)) return false;

        // Run each stage's setup(). Stages added after init() handle
        // their own setup in addStage().
        for (auto &s : m_stages) {
            if (!s->setup()) {
                SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                             "[Pipeline] stage '%s' setup() failed", s->name().c_str());
                return false;
            }
        }
        return true;
    }

    bool Pipeline::resize(int width, int height)
    {
        if (width == m_width && height == m_height) return true;
        m_width = width;
        m_height = height;
        if (!m_targetA.resize(width, height)) return false;
        if (!m_targetB.resize(width, height)) return false;
        for (auto &s : m_stages) s->onResize(width, height);
        return true;
    }

    void Pipeline::render(SpriteBatcher *batcher, float dt)
    {
        if (m_stages.empty() || !batcher) {
            m_lastOutput = nullptr;
            return;
        }

        // Ping-pong rotation: input <- previous output, output <- the
        // other target. The first stage has no real input — it's a
        // Scene stage that clears and draws fresh content.
        const RenderTarget *input = nullptr;
        RenderTarget *output = &m_targetA;

        for (size_t i = 0; i < m_stages.size(); ++i) {
            Stage *st = m_stages[i].get();
            if (!st->enabled) continue;

            FrameContext ctx;
            ctx.inputTarget = input;
            ctx.outputTarget = output;
            ctx.batcher = batcher;
            ctx.viewportWidth = m_width;
            ctx.viewportHeight = m_height;
            ctx.dt = dt;

            st->execute(ctx);

            // Rotate.
            input = output;
            output = (output == &m_targetA) ? &m_targetB : &m_targetA;
        }

        m_lastOutput = input;  // the last stage we actually ran wrote here
    }

    void Pipeline::addStage(std::unique_ptr<Stage> stage)
    {
        if (!stage) return;
        // If the pipeline is already initialised, run the new stage's setup.
        if (m_width > 0 && m_height > 0) {
            if (!stage->setup()) {
                SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                             "[Pipeline] late-added stage '%s' setup() failed",
                             stage->name().c_str());
                return;
            }
            stage->onResize(m_width, m_height);
        }
        m_stages.push_back(std::move(stage));
    }

    bool Pipeline::removeStage(const std::string &name)
    {
        auto it = std::find_if(m_stages.begin(), m_stages.end(),
            [&](const std::unique_ptr<Stage> &s) { return s->name() == name; });
        if (it == m_stages.end()) return false;
        m_stages.erase(it);
        return true;
    }

    bool Pipeline::moveStage(const std::string &name, int newIndex)
    {
        auto it = std::find_if(m_stages.begin(), m_stages.end(),
            [&](const std::unique_ptr<Stage> &s) { return s->name() == name; });
        if (it == m_stages.end()) return false;
        if (newIndex < 0) newIndex = 0;
        if (newIndex >= (int)m_stages.size()) newIndex = (int)m_stages.size() - 1;

        auto stage = std::move(*it);
        m_stages.erase(it);
        m_stages.insert(m_stages.begin() + newIndex, std::move(stage));
        return true;
    }

    Stage *Pipeline::findStage(const std::string &name)
    {
        for (auto &s : m_stages) if (s->name() == name) return s.get();
        return nullptr;
    }

}}
