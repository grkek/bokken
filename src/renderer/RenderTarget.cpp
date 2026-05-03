#include "RenderTarget.hpp"

namespace Bokken {
namespace Renderer {

    RenderTarget::~RenderTarget()
    {
        // Pointer guards removed — GLEW guarantees these are non-null
        // post-load, so checking them tripped -Wpointer-bool-conversion.
        if (m_depthRBO) glDeleteRenderbuffers(1, &m_depthRBO);
        if (m_fbo)      glDeleteFramebuffers(1, &m_fbo);
    }

    RenderTarget::RenderTarget(RenderTarget&& o) noexcept
        : m_fbo(o.m_fbo), m_depthRBO(o.m_depthRBO), m_color(std::move(o.m_color)),
          m_width(o.m_width), m_height(o.m_height),
          m_colorFormat(o.m_colorFormat), m_hasDepth(o.m_hasDepth)
    {
        o.m_fbo = 0;
        o.m_depthRBO = 0;
        o.m_width = o.m_height = 0;
    }

    RenderTarget& RenderTarget::operator=(RenderTarget&& o) noexcept
    {
        if (this != &o) {
            if (m_depthRBO) glDeleteRenderbuffers(1, &m_depthRBO);
            if (m_fbo)      glDeleteFramebuffers(1, &m_fbo);
            m_fbo = o.m_fbo; m_depthRBO = o.m_depthRBO;
            m_color = std::move(o.m_color);
            m_width = o.m_width; m_height = o.m_height;
            m_colorFormat = o.m_colorFormat; m_hasDepth = o.m_hasDepth;
            o.m_fbo = 0; o.m_depthRBO = 0;
        }
        return *this;
    }

    bool RenderTarget::create(int width, int height, TextureFormat colorFormat, bool withDepth)
    {
        if (width <= 0 || height <= 0) return false;

        m_colorFormat = colorFormat;
        m_hasDepth = withDepth;
        m_width = width;
        m_height = height;

        if (!m_color.create(width, height, colorFormat,
                            TextureFilter::Linear, TextureWrap::Clamp))
            return false;

        if (!m_fbo) glGenFramebuffers(1, &m_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, m_color.id(), 0);

        if (withDepth) {
            if (!m_depthRBO) glGenRenderbuffers(1, &m_depthRBO);
            glBindRenderbuffer(GL_RENDERBUFFER, m_depthRBO);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                      GL_RENDERBUFFER, m_depthRBO);
        }

        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                         "[RenderTarget] FBO incomplete: 0x%x", status);
            return false;
        }
        return true;
    }

    bool RenderTarget::resize(int width, int height)
    {
        if (width == m_width && height == m_height && m_fbo) return true;
        return create(width, height, m_colorFormat, m_hasDepth);
    }

    void RenderTarget::bind() const
    {
        if (!m_fbo) return;
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glViewport(0, 0, m_width, m_height);
    }

    void RenderTarget::bindDefault()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

}}
