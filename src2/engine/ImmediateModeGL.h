// OpenGL immediate mode backend of Legend of Grimrock 2, reconstructed from grimrock2.exe
// ImmediateModeGL.cpp (0x004e53f0-0x004e5a80): the vertices stream through the context's
// mapped buffer and a vertex array object.
#pragma once
#include "engine/ImmediateMode.h"
#include "engine/RenderContextGL.h"

namespace engine
{

class ImmediateModeGL : public ImmediateMode
{
  public:
    // 0x004e54e0
    ImmediateModeGL(RenderContextGL* context);
    // 0x004e5680
    ~ImmediateModeGL();
    void beginDraw();
    void endDraw();
    void setScissorRect(int x0, int y0, int x1, int y1);
    void setBlendMode(Material::BlendMode mode);
    void setTextureFilter(Material::TextureFilter filter);
    void setTextureAddress(Material::AddressMode mode);
    void getSize(int& width, int& height);
    void drawPoints(const IMVertex* verts, int count, const core::Matrix4x4& transform,
                    bool threeDee);
    void drawLines(const IMVertex* verts, int count, const core::Matrix4x4& transform,
                   bool threeDee);
    void drawTriangles(const IMVertex* verts, int count, const core::Matrix4x4& transform,
                       RenderableTexture* texture, bool threeDee);
    void drawRects(const IMVertex* verts, int count, const core::Matrix4x4& transform,
                   RenderableTexture* texture, bool threeDee);
    void drawAntialisedLines(const IMVertex* verts, int count, const core::Matrix4x4& transform,
                             float lineWidth);
    void drawRoundedRect(const core::Vec2& pos, const core::Vec2& size, float radius,
                         const core::Color& color);
    int getCaps();

  private:
    // 0x004e5710: primitive 0 points, 1 lines, 2 triangles, 3 quads.
    void drawInternal(int primitive, const IMVertex* verts, int count,
                      const core::Matrix4x4& transform, bool threeDee, RenderableTexture* texture);

    RenderContextGL* m_pContext;
    int m_width;
    int m_height;
    GLuint m_vertexArray;
    ShaderProgramGL* m_pProgram;
    ShaderProgramGL* m_pTexProgram;
    int m_blendMode;
    int m_textureFilter;
    int m_textureAddress;
};

} // namespace engine
