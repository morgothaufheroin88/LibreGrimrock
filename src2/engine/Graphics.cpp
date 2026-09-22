// Reconstructed from grimrock2.exe Graphics.cpp and the GraphicsGL functions of
// RenderContextGL.cpp.
#include "engine/Graphics.h"
#include "engine/Material.h"
#include "engine/RendererGL.h"

namespace engine
{

using namespace core;

Graphics* Graphics::sm_pActive = 0;

// 0x004e4350
GraphicsGL::GraphicsGL(RenderContextGL* context) : m_pContext(context)
{
    RenderableShaderGL* shader = new RenderableShaderGL;
    m_blitShader.reset(shader);
    shader->initPostProcessShader("shaders/gl/PostProcess.fsh");
    m_blitMaterial.reset(new Material(""));
    m_blitMaterial->setShader(shader);
}
// 0x004e4290
GraphicsGL::~GraphicsGL() {}

// 0x004e4000: colour attachment 0 = target texture, no depth/stencil.
void GraphicsGL::setRenderTarget(RenderableTexture* target)
{
    if (!target)
    {
        m_pContext->setRenderTarget(RenderContextGL::DefaultFrameBuffer);
        return;
    }
    TextureGL* texture = ((RenderableTextureGL*)target)->getTexture();
    m_pContext->setRenderTarget((Texture2DGL*)texture, 0, 0, 0);
}
// 0x004e3f50
void GraphicsGL::setViewport(int x, int y, int width, int height)
{
    glViewport(x, y, width, height);
    checkGLErrors("glViewport");
}
// 0x004e3f80
void GraphicsGL::clear(const Color& color)
{
    glClearColor(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
// 0x004e4030: post process material, program is the shader's first program.
void GraphicsGL::activateMaterial(const Material& material, const Matrix4x4& transform)
{
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    m_pContext->setBlendMode(material.getBlendMode());
    glDisable(GL_CULL_FACE);
    RenderableShaderGL* shader = (RenderableShaderGL*)material.getShader();
    m_pContext->useProgram(shader->getPostProcessProgram());
    m_pContext->setShaderParams(material, 0);
    GLint location = glGetUniformLocation(m_pContext->getProgram()->getProgram(), "g_transform");
    glUniformMatrix4fv(location, 1, GL_FALSE, transform.m);
}
// 0x004e41b0: quad covering pos/size of the renderer viewport, in clip space.
void GraphicsGL::drawRect(RenderableTexture& texture, const Vec2& pos, const Vec2& size,
                          Material* material)
{
    if (!material)
        material = m_blitMaterial.get();
    Renderer* renderer = Renderer::getActiveRenderer();
    float viewportWidth = (float)renderer->m_viewportWidth;
    float viewportHeight = (float)renderer->m_viewportHeight;
    Matrix4x4 transform;
    transform.makeIdentity();
    transform.m[0] = size.x / viewportWidth;
    transform.m[5] = size.y / viewportHeight;
    transform.m[12] = (pos.x / viewportWidth) * 2.0f + (transform.m[0] - 1.0f);
    transform.m[13] = (1.0f - transform.m[5]) - (pos.y / viewportHeight) * 2.0f;
    material->setTexture("g_sourceTex", &texture);
    activateMaterial(*material, transform);
    m_pContext->drawRect();
}
// 0x004e4100
void GraphicsGL::drawRect(RenderableTexture& texture, const Vec2& pos, Material* material)
{
    drawRect(texture, pos, Vec2((float)texture.getWidth(), (float)texture.getHeight()), material);
}
// 0x004e40d0
void GraphicsGL::drawRect(Material* material)
{
    if (!material)
        material = m_blitMaterial.get();
    Matrix4x4 identity;
    identity.makeIdentity();
    activateMaterial(*material, identity);
    m_pContext->drawRect();
}
// 0x004e4160
void GraphicsGL::blit(RenderableTexture& source, RenderableTexture* target, Material* material)
{
    if (!material)
        material = m_blitMaterial.get();
    setRenderTarget(target);
    material->setTexture("sourceTex", &source);
    Matrix4x4 identity;
    identity.makeIdentity();
    activateMaterial(*material, identity);
    m_pContext->drawRect();
}

} // namespace engine
