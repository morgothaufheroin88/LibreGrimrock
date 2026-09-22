// 2D/post process graphics interface of Legend of Grimrock 2, reconstructed from
// grimrock2.exe Graphics.cpp and the GraphicsGL part of RenderContextGL.cpp
// (0x004e3f50-0x004e4460). GraphicsGL owns its own blit shader and material.
#pragma once
#include "core/Color.h"
#include "core/Matrix.h"
#include "core/SharedPtr.h"
#include "core/Vector.h"

namespace engine
{

class RenderableTexture;
class RenderableShader;
class Material;
class RenderContextGL;

class Graphics
{
  public:
    virtual ~Graphics() {}
    virtual void setRenderTarget(RenderableTexture* target) = 0;
    virtual void setViewport(int x, int y, int width, int height) = 0;
    virtual void clear(const core::Color& color) = 0;
    // Texture at a pixel position and size of the renderer's viewport, null material =
    // plain copy.
    virtual void drawRect(RenderableTexture& texture, const core::Vec2& pos, const core::Vec2& size,
                          Material* material) = 0;
    virtual void drawRect(RenderableTexture& texture, const core::Vec2& pos,
                          Material* material) = 0;
    // Full screen quad with the material (null = blit material).
    virtual void drawRect(Material* material) = 0;
    virtual void blit(RenderableTexture& source, RenderableTexture* target, Material* material) = 0;

    static Graphics* sm_pActive;
};

class GraphicsGL : public Graphics
{
  public:
    // 0x004e4350
    GraphicsGL(RenderContextGL* context);
    // 0x004e4290
    ~GraphicsGL();
    void setRenderTarget(RenderableTexture* target);
    void setViewport(int x, int y, int width, int height);
    void clear(const core::Color& color);
    void drawRect(RenderableTexture& texture, const core::Vec2& pos, const core::Vec2& size,
                  Material* material);
    void drawRect(RenderableTexture& texture, const core::Vec2& pos, Material* material);
    void drawRect(Material* material);
    void blit(RenderableTexture& source, RenderableTexture* target, Material* material);
    // 0x004e4030: post process material with the given quad transform (g_transform).
    void activateMaterial(const Material& material, const core::Matrix4x4& transform);
    Material* getBlitMaterial() const
    {
        return m_blitMaterial.get();
    }

  private:
    RenderContextGL* m_pContext;
    core::SharedPtr<RenderableShader> m_blitShader;
    core::SharedPtr<Material> m_blitMaterial;
};

} // namespace engine
