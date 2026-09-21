// OpenGL immediate mode backend, reconstructed from ImmediateModeGL.cpp (0x08129000-0x0812a370).
#pragma once
#include "engine/ImmediateMode.h"
#include "engine/RenderContextGL.h"

namespace engine
{

class ImmediateModeGL : public ImmediateMode
{
  public:
    ImmediateModeGL(RenderContextGL* context);
    ~ImmediateModeGL();
    void beginDraw();
    void endDraw();
    void setScissorRect(int x0, int y0, int x1, int y1);
    void setBlendMode(Material::BlendMode mode);
    void getSize(int& width, int& height);
    void drawPoints(const IMVertex* verts, int count, const core::Matrix4x4& transform,
                    bool threeDee);
    void drawLines(const IMVertex* verts, int count, const core::Matrix4x4& transform,
                   bool threeDee);
    void drawTriangles(const IMVertex* verts, int count, const core::Matrix4x4& transform,
                       RenderableTexture* texture);
    void drawRects(const IMVertex* verts, int count, const core::Matrix4x4& transform,
                   RenderableTexture* texture);
    void drawAntialisedLines(const IMVertex* verts, int count, const core::Matrix4x4& transform,
                             float lineWidth);
    void drawRoundedRect(const core::Vec2& pos, const core::Vec2& size, float radius,
                         const core::Color& color);
    int getCaps();

  private:
    // 0x08129440: primitive 0 points, 1 lines, 2 triangles, 3 quads.
    void drawInternal(int primitive, const IMVertex* verts, int count,
                      const core::Matrix4x4& transform, bool threeDee, RenderableTexture* texture);

    RenderContextGL* m_pContext;
    int m_width;
    int m_height;
    ShaderProgramGL* m_pProgram;
    ShaderProgramGL* m_pTexProgram;
    int m_blendMode;
};

} // namespace engine
