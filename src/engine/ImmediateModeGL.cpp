// Reconstructed from Grimrock.bin.x86 ImmediateModeGL.cpp.
#include "engine/ImmediateModeGL.h"
#include "engine/RendererGL.h"

namespace engine
{

using namespace core;

// 0x081291f0
ImmediateModeGL::ImmediateModeGL(RenderContextGL* context)
    : m_pContext(context), m_width(0), m_height(0), m_pProgram(0), m_pTexProgram(0)
{
    m_pProgram =
        new ShaderProgramGL("shaders/gl/ImmediateMode.vsh", "shaders/gl/ImmediateMode.fsh");
    m_pTexProgram =
        new ShaderProgramGL("shaders/gl/ImmediateModeTex.vsh", "shaders/gl/ImmediateModeTex.fsh");
    m_blendMode = Material::Translucent;
    im::init(this);
}
// 0x081291b0
ImmediateModeGL::~ImmediateModeGL()
{
    delete m_pTexProgram;
    delete m_pProgram;
}
// 0x08129320: viewport of the active renderer, scissor test on.
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
// 0x08129070
void ImmediateModeGL::endDraw()
{
    glDisable(GL_SCISSOR_TEST);
}
// 0x08129090: inclusive rectangle in window coordinates with y down.
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
// 0x08129000
void ImmediateModeGL::setBlendMode(Material::BlendMode mode)
{
    m_blendMode = mode;
}
// 0x0812a340
void ImmediateModeGL::getSize(int& width, int& height)
{
    width = m_width;
    height = m_height;
}
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
                                    RenderableTexture* texture)
{
    drawInternal(RenderContextGL::Prim_Triangles, verts, count, transform, false, texture);
}
void ImmediateModeGL::drawRects(const IMVertex* verts, int count, const Matrix4x4& transform,
                                RenderableTexture* texture)
{
    drawInternal(RenderContextGL::Prim_Quads, verts, count, transform, false, texture);
}
// 0x08129010: no antialiasing support, plain lines.
void ImmediateModeGL::drawAntialisedLines(const IMVertex* verts, int count,
                                          const Matrix4x4& transform, float lineWidth)
{
    drawLines(verts, count, transform, false);
}
// 0x08129030
void ImmediateModeGL::drawRoundedRect(const Vec2& pos, const Vec2& size, float radius,
                                      const Color& color)
{
}
// 0x0812a360
int ImmediateModeGL::getCaps()
{
    return 0;
}

// 0x08129440
void ImmediateModeGL::drawInternal(int primitive, const IMVertex* verts, int count,
                                   const Matrix4x4& transform, bool threeDee,
                                   RenderableTexture* texture)
{
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(ShaderProgramGL::A_position);
    glEnableVertexAttribArray(ShaderProgramGL::A_texcoord);
    glEnableVertexAttribArray(ShaderProgramGL::A_color);
    glVertexAttribPointer(ShaderProgramGL::A_position, threeDee ? 3 : 2, GL_FLOAT, GL_FALSE,
                          sizeof(IMVertex), &verts[0].pos);
    glVertexAttribPointer(ShaderProgramGL::A_texcoord, 2, GL_FLOAT, GL_FALSE, sizeof(IMVertex),
                          &verts[0].u);
    glVertexAttribPointer(ShaderProgramGL::A_color, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(IMVertex),
                          &verts[0].color);
    m_pContext->useProgram(texture ? m_pTexProgram : m_pProgram);
    glDisable(GL_CULL_FACE);
    m_pContext->setBlendMode(m_blendMode);
    glDisable(GL_ALPHA_TEST);
    if (threeDee)
    {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
    }
    else
    {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
    }
    if (texture)
    {
        TextureGL* textureGL = ((RenderableTextureGL*)texture)->getTexture();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(textureGL->getTarget(), textureGL->getHandle());
        m_pContext->getProgram()->setUniform(ShaderProgramGL::U_texture, 0);
        m_pContext->setTextureFilterAndWrapMode(textureGL, -1, -1);
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
    m_pContext->drawArrays(primitive, 0, count);
    glDisableVertexAttribArray(ShaderProgramGL::A_position);
    glDisableVertexAttribArray(ShaderProgramGL::A_texcoord);
    glDisableVertexAttribArray(ShaderProgramGL::A_color);
}

} // namespace engine
