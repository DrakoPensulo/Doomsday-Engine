/** @file glframebuffer.cpp  GL frame buffer.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#include "de/GLFramebuffer"
#include "de/GuiApp"

#include <de/Log>
#include <de/Canvas>
#include <de/Drawable>
#include <de/GLInfo>
#include <de/Property>

namespace de {

DENG2_STATIC_PROPERTY(DefaultSampleCount, int)

DENG2_PIMPL(GLFramebuffer)
, DENG2_OBSERVES(DefaultSampleCount, Change)
, DENG2_OBSERVES(GuiApp, GLContextChange)
{
    Image::Format colorFormat;
    Size size;
    int _samples; ///< don't touch directly (0 == default)
    GLTarget target;
    GLTexture color;
    GLTexture depthStencil;

    Drawable bufSwap;
    GLUniform uMvpMatrix;
    GLUniform uBufTex;
    GLUniform uColor;
    typedef GLBufferT<Vertex2Tex> VBuf;

    Instance(Public *i)
        : Base(i)
        , colorFormat(Image::RGB_888)
        , _samples(0)
        , uMvpMatrix("uMvpMatrix", GLUniform::Mat4)
        , uBufTex   ("uTex",       GLUniform::Sampler2D)
        , uColor    ("uColor",     GLUniform::Vec4)
    {
        pDefaultSampleCount.audienceForChange() += this;
    }

    ~Instance()
    {
        pDefaultSampleCount.audienceForChange() -= this;
        release();
    }

    void appGLContextChanged()
    {
        /*
        qDebug() << "rebooting FB" << thisPublic << self.isReady() << target.glName() << target.isReady() << size.asText();
        self.glDeinit();
        self.glInit();
        */
    }

    int sampleCount() const
    {
        if(_samples <= 0) return pDefaultSampleCount;
        return _samples;
    }

    bool isMultisampled() const
    {
        return sampleCount() > 1;
    }

    void valueOfDefaultSampleCountChanged()
    {
        reconfigure();
    }

    void alloc()
    {
        /// @todo Create a separate program for stereo swaps that don't require MS.

        /// @todo Generate the shader on the fly with the correct number of samples
        /// and a fixed size/UV coords. Needs to be generated whenever the configuration
        /// changes.

        // Prepare the multisample-resolving blit.
        VBuf *buf = new VBuf;
        bufSwap.addBuffer(buf);
        bufSwap.program().build(
            // Vertex shader:
            Block("#version 330\n"
                  "uniform highp mat4 uMvpMatrix; "
                  "in highp vec4 aVertex; "
                  "in highp vec2 aUV; "
                  "out highp vec2 vUV; "
                  "void main(void) {"
                  "  gl_Position = uMvpMatrix * aVertex; "
                  "  vUV = aUV; }"),
            // Fragment shader:
            Block("#version 330\n"
                  "uniform sampler2DMS uTex; " // multisampled (assume 4)
                  "uniform highp vec4 uColor; "
                  "in highp vec2 vUV; "
                  "out highp vec4 FragColor; "
                  "void main(void) { "
                  "  ivec2 texSize = textureSize(uTex); "
                  "  ivec2 texUv = ivec2(int(texSize.x * vUV.s), int(texSize.y * vUV.t)); "
                  "  FragColor = uColor * 0.25 * "
                  "  ( texelFetch(uTex, texUv, 0)"
                  "  + texelFetch(uTex, texUv, 1)"
                  "  + texelFetch(uTex, texUv, 2)"
                  "  + texelFetch(uTex, texUv, 3) );"
                  "}"))
                << uMvpMatrix
                << uBufTex
                << uColor;

        buf->setVertices(gl::TriangleStrip,
                         VBuf::Builder().makeQuad(Rectanglef(0, 0, 1, 1),
                                                  Rectanglef(0, 1, 1, -1)),
                         gl::Static);

        uMvpMatrix = Matrix4f::ortho(0, 1, 0, 1);
        uColor     = Vector4f(1, 1, 1, 1);
        uBufTex    = color;
    }

    void release()
    {
        bufSwap.clear();
        color.clear();
        depthStencil.clear();
        target.configure();
    }

    void reconfigure()
    {
        if(!self.isReady() || size == Size()) return;

        int const glSamples = (isMultisampled()? sampleCount() : 0);

        LOGDEV_GL_VERBOSE("Reconfiguring framebuffer: %s ms:%i")
                << size.asText() << glSamples;

        // Configure textures for the framebuffer.
        GLPixelFormat fmt = Image::glFormat(colorFormat);
        fmt.samples = glSamples;
        color.setUndefinedContent(size, fmt);
        color.setWrap(gl::ClampToEdge, gl::ClampToEdge);
        color.setFilter(gl::Nearest, gl::Linear, gl::MipNone);

        depthStencil.setDepthStencilContent(size, glSamples);
        depthStencil.setWrap(gl::ClampToEdge, gl::ClampToEdge);
        depthStencil.setFilter(gl::Nearest, gl::Nearest, gl::MipNone);

        try
        {
            // We'd like to use texture attachments for both color and depth/stencil.
            target.configure(&color, &depthStencil);
        }
        catch(GLTarget::ConfigError const &er)
        {
            // Alternatively try without depth/stencil texture (some renderer features
            // will not be available!).
            LOG_GL_WARNING("Texture-based framebuffer failed: %s\n"
                           "Trying fallback without depth/stencil texture")
                    << er.asText();

            target.configure(GLTarget::Color, color, GLTarget::DepthStencil);
        }

        target.clear(GLTarget::ColorDepthStencil);
    }

    void resize(Size const &newSize)
    {
        if(size != newSize)
        {
            size = newSize;
            reconfigure();
        }
    }

    void drawSwap()
    {
        bufSwap.draw();
    }

    void swapBuffers(Canvas &canvas, gl::SwapBufferMode swapMode)
    {
        GLTarget defaultTarget;

        GLState::push()
                .setTarget(defaultTarget)
                .setViewport(Rectangleui::fromSize(size))
                .apply();

        switch(swapMode)
        {
        case gl::SwapMonoBuffer:
            if(isMultisampled())
            {
                drawSwap();
            }
            else
            {
                target.blit(defaultTarget);  // copy to system backbuffer
            }
            canvas.QGLWidget::swapBuffers();
            break;

        case gl::SwapWithAlpha:
            drawSwap();
            break;

        case gl::SwapStereoLeftBuffer:
            glDrawBuffer(GL_BACK_LEFT);
            drawSwap();
            glDrawBuffer(GL_BACK);
            break;

        case gl::SwapStereoRightBuffer:
            glDrawBuffer(GL_BACK_RIGHT);
            drawSwap();
            glDrawBuffer(GL_BACK);
            break;

        case gl::SwapStereoBuffers:
            canvas.QGLWidget::swapBuffers();
            break;
        }

        GLState::pop().apply();
    }
};

GLFramebuffer::GLFramebuffer(Image::Format const &colorFormat, Size const &initialSize, int sampleCount)
    : d(new Instance(this))
{
    d->colorFormat = colorFormat;
    d->size        = initialSize;
    d->_samples    = sampleCount;
}

void GLFramebuffer::glInit()
{
    if(isReady()) return;

    LOG_AS("GLFramebuffer");

    // Check for some integral OpenGL functionality.
    /*if(!GLInfo::extensions().ARB_framebuffer_object)
    {
        LOG_GL_WARNING("Required GL_ARB_framebuffer_object is missing!");
    }
    if(!GLInfo::extensions().EXT_packed_depth_stencil)
    {
        LOG_GL_WARNING("GL_EXT_packed_depth_stencil is missing, some features may be unavailable");
    }*/

    d->alloc();
    setState(Ready);

    d->reconfigure();

    LIBGUI_ASSERT_GL_OK();
}

void GLFramebuffer::glDeinit()
{
    setState(NotReady);
    d->release();
}

void GLFramebuffer::setSampleCount(int sampleCount)
{
    if(!GLInfo::isFramebufferMultisamplingSupported())
    {
        sampleCount = 1;
    }

    if(d->_samples != sampleCount)
    {
        LOG_AS("GLFramebuffer");

        d->_samples = sampleCount;
        d->reconfigure();
    }
}

void GLFramebuffer::setColorFormat(Image::Format const &colorFormat)
{
    if(d->colorFormat != colorFormat)
    {
        d->colorFormat = colorFormat;
        d->reconfigure();
    }
}

void GLFramebuffer::resize(Size const &newSize)
{
    d->resize(newSize);
}

GLFramebuffer::Size GLFramebuffer::size() const
{
    return d->size;
}

GLTarget &GLFramebuffer::target() const
{
    return d->target;
}

GLTexture &GLFramebuffer::colorTexture() const
{
    return d->color;
}

GLTexture &GLFramebuffer::depthStencilTexture() const
{
    return d->depthStencil;
}

int GLFramebuffer::sampleCount() const
{
    return d->sampleCount();
}

void GLFramebuffer::swapBuffers(Canvas &canvas, gl::SwapBufferMode swapMode)
{
    d->swapBuffers(canvas, swapMode);
}

void GLFramebuffer::drawBuffer(float opacity)
{
    d->uColor = Vector4f(1, 1, 1, opacity);
    GLState::push()
            .setCull(gl::None)
            .setDepthTest(false)
            .setDepthWrite(false);
    d->drawSwap();
    GLState::pop();
    d->uColor = Vector4f(1, 1, 1, 1);
}

bool GLFramebuffer::setDefaultMultisampling(int sampleCount)
{   
    LOG_AS("GLFramebuffer");

    int const newCount = max(1, sampleCount);
    if(pDefaultSampleCount != newCount)
    {
        pDefaultSampleCount = newCount;
        return true;
    }
    return false;
}

int GLFramebuffer::defaultMultisampling()
{
    return pDefaultSampleCount;
}

} // namespace de
