// Batched 2D/3D immediate mode drawing of Legend of Grimrock 2, reconstructed from
// grimrock2.exe ImmediateMode.cpp (0x004bc480-0x004bea10). The state stack also carries the
// texture filter and address modes and the transform can be built incrementally.
#pragma once
#include "core/Color.h"
#include "core/Matrix.h"
#include "core/Vector.h"
#include "engine/Material.h"

namespace engine
{

class Font;
class RenderableTexture;

// 24 bytes: position, texcoord, colour.
struct IMVertex
{
    core::Vec3 pos;
    float u, v;
    core::Color color;
};

// Backend interface (0x0812a330). Vertex arrays are handed over per primitive type.
class ImmediateMode
{
  public:
    enum Caps
    {
        Caps_RoundedRect = 1
    };
    virtual ~ImmediateMode() {}
    virtual void beginDraw() = 0;
    virtual void endDraw() = 0;
    virtual void setScissorRect(int x0, int y0, int x1, int y1) = 0;
    virtual void setBlendMode(Material::BlendMode mode) = 0;
    virtual void setTextureFilter(Material::TextureFilter filter) = 0;
    virtual void setTextureAddress(Material::AddressMode mode) = 0;
    virtual void getSize(int& width, int& height) = 0;
    virtual void drawPoints(const IMVertex* verts, int count, const core::Matrix4x4& transform,
                            bool threeDee) = 0;
    virtual void drawLines(const IMVertex* verts, int count, const core::Matrix4x4& transform,
                           bool threeDee) = 0;
    virtual void drawTriangles(const IMVertex* verts, int count, const core::Matrix4x4& transform,
                               RenderableTexture* texture, bool threeDee) = 0;
    virtual void drawRects(const IMVertex* verts, int count, const core::Matrix4x4& transform,
                           RenderableTexture* texture, bool threeDee) = 0;
    virtual void drawAntialisedLines(const IMVertex* verts, int count,
                                     const core::Matrix4x4& transform, float lineWidth) = 0;
    virtual void drawRoundedRect(const core::Vec2& pos, const core::Vec2& size, float radius,
                                 const core::Color& color) = 0;
    virtual int getCaps() = 0;
};

namespace im
{

enum ShapeType
{
    Shape_Points = 0,
    Shape_Lines = 1,
    Shape_LinesAA = 2,
    Shape_Triangles = 3,
    Shape_Rects = 4
};

// drawImage flags
enum
{
    Rotate90 = 1,
    Rotate180 = 2,
    Rotate270 = 3,
    FlipX = 4,
    FlipY = 8
};

void init(ImmediateMode* backend);
// Resets the state stack for a frame of the given size (0x08106f60).
void prepare(int width, int height);
void beginDraw();
void endDraw();
void pushState();
bool popState();
void setTransform(const core::Matrix4x4& m);
void setIdentityTransform();
// 0x004bcb70 / 0x004bcbe0 / 0x004bcc50: transform = transform * T/S/R
void translate(const core::Vec3& v);
void scale(const core::Vec3& v);
void rotate(float angle, const core::Vec3& axis);
void setLineWidth(float width);
void clipTo(int x0, int y0, int x1, int y1);
void resetClip();
void getClipRect(int* x0, int* y0, int* x1, int* y1);
void setBlendMode(Material::BlendMode mode);
void setTextureFilterMode(Material::TextureFilter filter);
void setTextureAddressMode(Material::AddressMode mode);

void beginShape(int type, RenderableTexture* texture, bool threeDee);
void vertexPosition(float x, float y);
void vertexPosition(float x, float y, float z);
void vertexTexcoord(float u, float v);
void vertexColor(const core::Color& color);
void endShape();

void drawPoint(int x, int y, const core::Color& color);
void drawPoint(const core::Vec2& p, const core::Color& color);
void drawPoint(const core::Vec3& p, const core::Color& color);
void drawLine(int x0, int y0, int x1, int y1, const core::Color& color);
void drawLine(const core::Vec2& a, const core::Vec2& b, const core::Color& color);
void drawLine(const core::Vec3& a, const core::Vec3& b, const core::Color& color);
void drawLineAA(int x0, int y0, int x1, int y1, const core::Color& color);
void drawLineAA(const core::Vec2& a, const core::Vec2& b, const core::Color& color);
void drawPolyLine(const core::Vec2* points, int count, bool closed, const core::Color& color);
void drawPolyLineAA(const core::Vec2* points, int count, bool closed, const core::Color& color);
void drawRect(int x, int y, int width, int height, const core::Color& color);
void drawRect(const core::Vec2& pos, const core::Vec2& size, const core::Color& color);
void fillRect(int x, int y, int width, int height, const core::Color& color);
void fillRect(const core::Vec2& pos, const core::Vec2& size, const core::Color& color);
void drawRoundedRect(int x, int y, int width, int height, float radius, const core::Color& color);
void fillRoundedRect(int x, int y, int width, int height, float radius, const core::Color& color);
void fillRoundedRectAA(int x, int y, int width, int height, float radius, const core::Color& color);
void drawEllipse(int x0, int y0, int x1, int y1, int segments, const core::Color& color);
void drawEllipse(const core::Vec2& mn, const core::Vec2& mx, int segments,
                 const core::Color& color);
void fillPolygon(const core::Vec2* points, int count, const core::Color& color);
void fillPolygonAA(const core::Vec2* points, int count, const core::Color& color);
void drawImage(RenderableTexture& texture, const core::Vec2& pos, const core::Vec2& size,
               const core::Vec2& uv0, const core::Vec2& uv1, const core::Color& color, int flags);
void drawImage(RenderableTexture& texture, const core::Vec2& pos, const core::Vec2& size,
               const core::Color& color, int flags);
void drawImage(RenderableTexture& texture, int x, int y, int srcX, int srcY, int srcWidth,
               int srcHeight, int width, int height, const core::Color& color, int flags);
void drawImage(RenderableTexture& texture, int x, int y, int srcX, int srcY, int srcWidth,
               int srcHeight, const core::Color& color, int flags);
void drawImage(RenderableTexture& texture, int x, int y, const core::Color& color, int flags);
void drawText(const char* text, const core::Vec2& pos, Font* font, const core::Color& color,
              int maxChars);
void drawText(const char* text, int x, int y, Font* font, const core::Color& color, int maxChars);
// 0x004be5d0: word wrapped text inside width; returns the widest line and the y below
// the last line when asked for.
void drawParagraph(const char* text, int x, int y, int width, Font* font, const core::Color& color,
                   int* maxLineWidth, int* endY);

// Draws and clears the core::DebugDraw queues (0x08106440).
void flushDebugDraw(const core::Matrix4x4& viewProj, Font* font);

} // namespace im
} // namespace engine
