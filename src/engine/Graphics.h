// 2D/post process graphics interface, reconstructed from Graphics.cpp (0x08107a20) and
// the GraphicsGL part of RenderContextGL.cpp (0x08117f00-0x0811a300).
#pragma once
#include "core/Color.h"

namespace engine
{

class RenderableTexture;
class Material;
class RenderContextGL;

class Graphics
{
  public:
    virtual ~Graphics() {}
    virtual void setRenderTarget(RenderableTexture* target) = 0;
    virtual void activateMaterial(const Material& material) = 0;
    virtual void clear(const core::Color& color) = 0;
    virtual void drawRect() = 0;
    virtual void blit(RenderableTexture& source, RenderableTexture* target, Material* material) = 0;

    static Graphics* sm_pActive;
};

class GraphicsGL : public Graphics
{
  public:
    GraphicsGL(RenderContextGL* context);
    ~GraphicsGL() {}
    void setRenderTarget(RenderableTexture* target);
    void activateMaterial(const Material& material);
    void clear(const core::Color& color);
    void drawRect();
    void blit(RenderableTexture& source, RenderableTexture* target, Material* material);

  private:
    RenderContextGL* m_pContext;
};

} // namespace engine
