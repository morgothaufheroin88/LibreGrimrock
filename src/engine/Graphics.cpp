// Reconstructed from Graphics.cpp and the GraphicsGL functions of RenderContextGL.cpp.
#include "engine/Graphics.h"
#include "engine/Material.h"
#include "engine/RendererGL.h"

namespace engine
{

using namespace core;

Graphics* Graphics::sm_pActive = 0;

// 0x08117f00
GraphicsGL::GraphicsGL(RenderContextGL* context) : m_pContext(context) {}

// 0x08118090: colour attachment 0 = target texture, no depth/stencil.
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

// 0x081187a0: post process material, program is the shader's first program.
void GraphicsGL::activateMaterial(const Material& material)
{
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    m_pContext->setBlendMode(RenderContextGL::Blend_Opaque);
    RenderableShaderGL* shader = (RenderableShaderGL*)material.getShader();
    m_pContext->useProgram(shader ? shader->getPostProcessProgram() : 0);
    m_pContext->resetTextureUnits();
    m_pContext->setShaderParams(material);
}

// 0x08118020
void GraphicsGL::clear(const Color& color)
{
    glClearColor(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

// 0x08119970: fullscreen quad through attribute 0.
void GraphicsGL::drawRect()
{
    static constexpr float verts[8] = {-1, 1, 1, 1, 1, -1, -1, -1};
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(ShaderProgramGL::A_position);
    glVertexAttribPointer(ShaderProgramGL::A_position, 2, GL_FLOAT, GL_FALSE, 8, verts);
    glDrawArrays(GL_QUADS, 0, 4);
    glDisableVertexAttribArray(ShaderProgramGL::A_position);
}

// 0x08117fb0
void GraphicsGL::blit(RenderableTexture& source, RenderableTexture* target, Material* material)
{
    if (!material)
        material = CommonResourcesGL::BlitMaterial;
    setRenderTarget(target);
    material->setTexture("sourceTex", &source);
    activateMaterial(*material);
    drawRect();
}

} // namespace engine
