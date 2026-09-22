// Reconstructed from grimrock2.exe ImmediateModeGL.cpp.
#include "engine/ImmediateModeGL.h"
#include "engine/RendererGL.h"
#include <cstring>

namespace engine
{

using namespace core;

// 0x004e54e0
ImmediateModeGL::ImmediateModeGL(RenderContextGL* context)
    : m_pContext(context), m_width(0), m_height(0), m_vertexArray(0), m_pProgram(0),
      m_pTexProgram(0)
{
    m_pProgram =
        new ShaderProgramGL("shaders/gl/ImmediateMode.vsh", "shaders/gl/ImmediateMode.fsh");
    m_pTexProgram =
        new ShaderProgramGL("shaders/gl/ImmediateModeTex.vsh", "shaders/gl/ImmediateModeTex.fsh");
    m_blendMode = Material::Translucent;
    m_textureFilter = Material::Linear_MipNearest;
    m_textureAddress = Material::Wrap;
    glGenVertexArrays(1, &m_vertexArray);
    glBindVertexArray(m_vertexArray);
    glEnableVertexAttribArray(ShaderProgramGL::A_position);
    glEnableVertexAttribArray(ShaderProgramGL::A_texcoord);
    glEnableVertexAttribArray(ShaderProgramGL::A_color);
    glBindVertexArray(0);
    im::init(this);
}
// 0x004e5680
ImmediateModeGL::~ImmediateModeGL()
{
    glDeleteVertexArrays(1, &m_vertexArray);
    delete m_pTexProgram;
    delete m_pProgram;
}
// 0x004e53f0: viewport of the active renderer, scissor test on.
void ImmediateModeGL::beginDraw()
{
    RendererGL* renderer = (RendererGL*)Renderer::sm_pActiveRenderer;
    m_width = renderer->m_viewportWidth;
    m_height = renderer->m_viewportHeight;
    glViewport(renderer->m_viewportX,
               renderer->getConfig().height - renderer->m_viewportY - m_height, m_width, m_height);
    checkGLErrors("glViewport");
    glEnable(GL_SCISSOR_TEST);
}
// 0x004e5440
void ImmediateModeGL::endDraw()
{
    glDisable(GL_SCISSOR_TEST);
}
// 0x004e5480: inclusive rectangle in window coordinates with y down.
void ImmediateModeGL::setScissorRect(int x0, int y0, int x1, int y1)
{
    RendererGL* renderer = (RendererGL*)Renderer::sm_pActiveRenderer;
    int w = x1 - x0 + 1, h = y1 - y0 + 1;
    if (h < 0)
        h = 0;
    if (w < 0)
        w = 0;
    glScissor(x0, renderer->getConfig().height - y1, w, h);
    checkGLErrors("glScissor");
}
// 0x004e5630 / 0x004e5640 / 0x004e5650
void ImmediateModeGL::setBlendMode(Material::BlendMode mode)
{
    m_blendMode = mode;
}
void ImmediateModeGL::setTextureFilter(Material::TextureFilter filter)
{
    m_textureFilter = filter;
}
void ImmediateModeGL::setTextureAddress(Material::AddressMode mode)
{
    m_textureAddress = mode;
}
// 0x004e5660
void ImmediateModeGL::getSize(int& width, int& height)
{
    width = m_width;
    height = m_height;
}
// 0x004e59e0-0x004e5a50
void ImmediateModeGL::drawPoints(const IMVertex* verts, int count, const Matrix4x4& transform,
                                 bool threeDee)
{
    drawInternal(RenderContextGL::Prim_Points, verts, count, transform, threeDee, 0);
}
void ImmediateModeGL::drawLines(const IMVertex* verts, int count, const Matrix4x4& transform,
                                bool threeDee)
{
    drawInternal(RenderContextGL::Prim_Lines, verts, count, transform, threeDee, 0);
}
void ImmediateModeGL::drawTriangles(const IMVertex* verts, int count, const Matrix4x4& transform,
                                    RenderableTexture* texture, bool threeDee)
{
    drawInternal(RenderContextGL::Prim_Triangles, verts, count, transform, threeDee, texture);
}
void ImmediateModeGL::drawRects(const IMVertex* verts, int count, const Matrix4x4& transform,
                                RenderableTexture* texture, bool threeDee)
{
    drawInternal(RenderContextGL::Prim_Quads, verts, count, transform, threeDee, texture);
}
// 0x004e5450: no antialiasing support, plain lines.
void ImmediateModeGL::drawAntialisedLines(const IMVertex* verts, int count,
                                          const Matrix4x4& transform, float lineWidth)
{
    drawLines(verts, count, transform, false);
}
// 0x004e5470
void ImmediateModeGL::drawRoundedRect(const Vec2& pos, const Vec2& size, float radius,
                                      const Color& color)
{
}
// 0x004aa880
int ImmediateModeGL::getCaps()
{
    return 0;
}

// 0x004e5710
void ImmediateModeGL::drawInternal(int primitive, const IMVertex* verts, int count,
                                   const Matrix4x4& transform, bool threeDee,
                                   RenderableTexture* texture)
{
    m_pContext->useProgram(texture ? m_pTexProgram : m_pProgram);
    glDisable(GL_CULL_FACE);
    m_pContext->setBlendMode(m_blendMode);
    if (threeDee)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    glDepthMask(threeDee ? GL_TRUE : GL_FALSE);
    if (texture)
    {
        TextureGL* textureGL = ((RenderableTextureGL*)texture)->getTexture();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(textureGL->getTarget(), textureGL->getHandle());
        m_pContext->getProgram()->setUniform(ShaderProgramGL::U_texture, 0);
        m_pContext->setTextureFilterAndWrapMode(textureGL, m_textureFilter, m_textureAddress);
    }
    Matrix4x4 mvp;
    if (!threeDee)
    {
        // pixel coordinates to clip space, points and lines get a half pixel offset
        float sx = 2.0f / m_width, sy = -2.0f / m_height;
        float ox, oy;
        if (primitive < RenderContextGL::Prim_Triangles)
        {
            ox = 1.0f / m_width - 1.0f;
            oy = 1.0f - 1.0f / m_height;
        }
        else
        {
            ox = -1.0f;
            oy = 1.0f;
        }
        Matrix4x4 ortho(sx, 0, 0, ox, 0, sy, 0, oy, 0, 0, 1, 0, 0, 0, 0, 1);
        mvp = RenderContextGL::sm_d3dToGLProj * (ortho * transform);
    }
    else
    {
        mvp = RenderContextGL::sm_d3dToGLProj * transform;
    }
    m_pContext->getProgram()->setUniform(ShaderProgramGL::U_modelViewProj, mvp);
    glBindVertexArray(m_vertexArray);
    int offset = m_pContext->getStreamOffset();
    void* data = m_pContext->streamWrite(count * (int)sizeof(IMVertex));
    if (!data)
        return;
    // streamWrite may have restarted the buffer
    offset = m_pContext->getStreamOffset() - count * (int)sizeof(IMVertex);
    memcpy(data, verts, count * sizeof(IMVertex));
    const char* base = (const char*)(intptr_t)offset;
    glVertexAttribPointer(ShaderProgramGL::A_position, 3, GL_FLOAT, GL_FALSE, sizeof(IMVertex),
                          base);
    glVertexAttribPointer(ShaderProgramGL::A_texcoord, 2, GL_FLOAT, GL_FALSE, sizeof(IMVertex),
                          base + 12);
    glVertexAttribPointer(ShaderProgramGL::A_color, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(IMVertex),
                          base + 20);
    m_pContext->streamDrawPrimitive(primitive, count);
}

} // namespace engine
