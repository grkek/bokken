#include "Texture2D.hpp"

namespace Bokken
{
    namespace Renderer
    {

        namespace
        {
            struct GLFormat
            {
                GLenum internalFormat;
                GLenum format;
                GLenum type;
                int bytesPerPixel;
            };

            GLFormat resolveFormat(TextureFormat f)
            {
                switch (f)
                {
                case TextureFormat::RGBA8:
                    return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4};
                case TextureFormat::R8:
                    return {GL_R8, GL_RED, GL_UNSIGNED_BYTE, 1};
                case TextureFormat::RGBA16F:
                    return {GL_RGBA16F, GL_RGBA, GL_FLOAT, 8};
                }
                return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4};
            }

            GLint resolveFilter(TextureFilter f)
            {
                return f == TextureFilter::Nearest ? GL_NEAREST : GL_LINEAR;
            }
            GLint resolveWrap(TextureWrap w)
            {
                return w == TextureWrap::Repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
            }
        }

        Texture2D::~Texture2D()
        {
            // With GLEW, glDeleteTextures is unconditionally valid post-load,
            // so the old `&& glDeleteTextures` guard fired the
            // -Wpointer-bool-conversion warning. The remaining `m_id` check
            // is the meaningful one.
            if (m_id)
                glDeleteTextures(1, &m_id);
        }

        Texture2D::Texture2D(Texture2D &&o) noexcept
            : m_id(o.m_id), m_width(o.m_width), m_height(o.m_height), m_format(o.m_format)
        {
            o.m_id = 0;
            o.m_width = o.m_height = 0;
        }

        Texture2D &Texture2D::operator=(Texture2D &&o) noexcept
        {
            if (this != &o)
            {
                if (m_id)
                    glDeleteTextures(1, &m_id);
                m_id = o.m_id;
                m_width = o.m_width;
                m_height = o.m_height;
                m_format = o.m_format;
                o.m_id = 0;
                o.m_width = o.m_height = 0;
            }
            return *this;
        }

        bool Texture2D::create(int width, int height, TextureFormat fmt,
                               TextureFilter filter, TextureWrap wrap)
        {
            if (width <= 0 || height <= 0)
                return false;
            if (!m_id)
                glGenTextures(1, &m_id);

            m_width = width;
            m_height = height;
            m_format = fmt;
            const GLFormat gf = resolveFormat(fmt);

            glBindTexture(GL_TEXTURE_2D, m_id);
            // R8 needs 1-byte alignment; default 4 truncates rows whose width
            // isn't a multiple of 4 — a classic font-atlas bug.
            glPixelStorei(GL_UNPACK_ALIGNMENT, gf.bytesPerPixel == 1 ? 1 : 4);
            glTexImage2D(GL_TEXTURE_2D, 0, gf.internalFormat, width, height, 0,
                         gf.format, gf.type, nullptr);
            const GLint flt = resolveFilter(filter);
            const GLint wr = resolveWrap(wrap);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, flt);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, flt);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wr);
            return true;
        }

        void Texture2D::upload(int x, int y, int width, int height, const void *data)
        {
            if (!m_id || !data)
                return;
            const GLFormat gf = resolveFormat(m_format);
            glBindTexture(GL_TEXTURE_2D, m_id);
            glPixelStorei(GL_UNPACK_ALIGNMENT, gf.bytesPerPixel == 1 ? 1 : 4);
            glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, gf.format, gf.type, data);
        }

        bool Texture2D::uploadFull(int width, int height, TextureFormat fmt, const void *data,
                                   TextureFilter filter, TextureWrap wrap)
        {
            if (width <= 0 || height <= 0)
                return false;
            if (!m_id)
                glGenTextures(1, &m_id);

            m_width = width;
            m_height = height;
            m_format = fmt;
            const GLFormat gf = resolveFormat(fmt);

            glBindTexture(GL_TEXTURE_2D, m_id);
            glPixelStorei(GL_UNPACK_ALIGNMENT, gf.bytesPerPixel == 1 ? 1 : 4);
            glTexImage2D(GL_TEXTURE_2D, 0, gf.internalFormat, width, height, 0,
                         gf.format, gf.type, data);
            const GLint flt = resolveFilter(filter);
            const GLint wr = resolveWrap(wrap);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, flt);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, flt);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wr);
            return true;
        }

        void Texture2D::setFilter(TextureFilter filter)
        {
            if (!m_id)
                return;
            const GLint flt = resolveFilter(filter);
            glBindTexture(GL_TEXTURE_2D, m_id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, flt);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, flt);
        }

        void Texture2D::setWrap(TextureWrap wrap)
        {
            if (!m_id)
                return;
            const GLint wr = resolveWrap(wrap);
            glBindTexture(GL_TEXTURE_2D, m_id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wr);
        }

        void Texture2D::bind(int unit) const
        {
            if (!m_id)
                return;
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, m_id);
        }

        void Texture2D::unbind(int unit)
        {
            // GLEW guarantees these entry points exist post-load, so the
            // old null-pointer guard is gone. Callers must still ensure a
            // GL context is current at the time of the call.
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

    }
}
