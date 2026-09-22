// Reconstructed from Grimrock.bin.x86 ImmediateMode.cpp.
#include "engine/ImmediateMode.h"
#include "core/Array.h"
#include "core/DebugDraw.h"
#include "core/Math.h"
#include "engine/Font.h"
#include "engine/Texture.h"
#include <climits>
#include <cmath>
#include <cstring>

namespace engine
{

using namespace core;

namespace
{

// 88 bytes in the original.
struct State
{
    Matrix4x4 transform;
    float lineWidth;
    int clipX0, clipY0, clipX1, clipY1;
    int blendMode;
};

Array<State> g_stack;
Array<IMVertex> g_buffer;
int g_nextVertex = 0;
ImmediateMode* g_pImmediateMode = 0;
bool g_batchStarted = false;
int g_primType = -1;
RenderableTexture* g_pTexture = 0;
bool g_threeDee = false;
IMVertex* g_pCurrentVertex = 0;

constexpr int NumCircleVerts = 32;     // unit circle samples for rounded corners
constexpr int RoundedCornerPoints = 9; // points per rounded rectangle corner
constexpr int RoundedRectPoints = RoundedCornerPoints * 4;
constexpr int VertexBufferGranularity = 0x1000; // g_buffer grows in this many vertices
constexpr float MaxOutlineNormalScale = 10.0f;  // rim width limit at sharp corners
Vec2 g_circleVerts[NumCircleVerts];

State& top()
{
    return g_stack[g_stack.size() - 1];
}

// 0x08103110: hands the batched vertices to the backend.
void flush()
{
    if (g_nextVertex > 0)
    {
        const State& s = top();
        switch (g_primType)
        {
        case im::Shape_Points:
            g_pImmediateMode->drawPoints(g_buffer.data(), g_nextVertex, s.transform, g_threeDee);
            break;
        case im::Shape_Lines:
            g_pImmediateMode->drawLines(g_buffer.data(), g_nextVertex, s.transform, g_threeDee);
            break;
        case im::Shape_LinesAA:
            g_pImmediateMode->drawAntialisedLines(g_buffer.data(), g_nextVertex, s.transform,
                                                  s.lineWidth);
            break;
        case im::Shape_Triangles:
            g_pImmediateMode->drawTriangles(g_buffer.data(), g_nextVertex, s.transform, g_pTexture);
            break;
        case im::Shape_Rects:
            g_pImmediateMode->drawRects(g_buffer.data(), g_nextVertex, s.transform, g_pTexture);
            break;
        }
    }
    g_nextVertex = 0;
    g_primType = -1;
    g_pTexture = 0;
}

// 0x08103710: grows the vertex buffer in 4096 vertex steps.
IMVertex* allocVertex(int count)
{
    int first = g_nextVertex;
    int needed = first + count;
    if (needed > g_buffer.size())
    {
        int size = (needed + VertexBufferGranularity - 1) & ~(VertexBufferGranularity - 1);
        g_buffer.resize(size);
    }
    g_nextVertex = needed;
    return g_buffer.data() + first;
}

// Starts a new batch when the primitive type, texture or space changes.
void select(int primType, RenderableTexture* texture, bool threeDee)
{
    if (g_primType != primType || g_pTexture != texture || g_threeDee != threeDee)
    {
        flush();
        g_primType = primType;
        g_pTexture = texture;
        g_threeDee = threeDee;
    }
}

inline void setVertex(IMVertex& v, float x, float y, float z, const Color& color)
{
    v.pos.set(x, y, z);
    v.color = color;
}

// RoundedRectPoints points around a rounded rectangle, RoundedCornerPoints per corner
// (0x08105430).
void roundedRectPoints(int x, int y, int width, int height, float radius, Vec2* points)
{
    Vec2 centers[4] = {Vec2(x + width - radius, y + height - radius),
                       Vec2(x + radius, y + height - radius), Vec2(x + radius, y + radius),
                       Vec2(x + width - radius, y + radius)};
    for (int corner = 0; corner < 4; ++corner)
        for (int j = 0; j < RoundedCornerPoints; ++j)
            points[corner * RoundedCornerPoints + j] =
                centers[corner] +
                g_circleVerts[(corner * (NumCircleVerts / 4) + j) % NumCircleVerts] * radius;
}

} // namespace

namespace im
{

// 0x081038d0
void init(ImmediateMode* backend)
{
    g_batchStarted = false;
    g_primType = -1;
    g_pTexture = 0;
    g_pImmediateMode = backend;
    g_threeDee = false;
    g_buffer.clear();
    g_buffer.reserve(0);
    g_nextVertex = 0;
    for (int i = 0; i < NumCircleVerts; ++i)
    {
        float angle = i * (1.0f / NumCircleVerts) * PI * 2.0f;
        g_circleVerts[i].set(std::cos(angle), std::sin(angle));
    }
}

// 0x08106f60
void prepare(int width, int height)
{
    g_stack.clear();
    g_stack.reserve(16);
    State state;
    state.transform = Matrix4x4::sm_mIdentity;
    state.lineWidth = 1.0f;
    state.clipX0 = 0;
    state.clipY0 = 0;
    state.clipX1 = width;
    state.clipY1 = height;
    state.blendMode = Material::Translucent;
    g_stack.push_back(state);
}

// 0x08103560
void beginDraw()
{
    g_pImmediateMode->beginDraw();
    g_batchStarted = true;
}

// 0x08103580
void endDraw()
{
    flush();
    g_batchStarted = false;
    g_pImmediateMode->endDraw();
}

// 0x081039a0: duplicates the top state.
void pushState()
{
    State copy = top();
    g_stack.push_back(copy);
}

// 0x081034e0
bool popState()
{
    flush();
    if (g_stack.size() <= 1)
        return false;
    g_stack.pop_back();
    const State& s = top();
    g_pImmediateMode->setScissorRect(s.clipX0, s.clipY0, s.clipX1, s.clipY1);
    g_pImmediateMode->setBlendMode((Material::BlendMode)s.blendMode);
    return true;
}

// 0x08103270
void setTransform(const Matrix4x4& m)
{
    flush();
    top().transform = m;
}

// 0x08103bc0
void setIdentityTransform()
{
    flush();
    top().transform = Matrix4x4::sm_mIdentity;
}

// 0x08103310
void setLineWidth(float width)
{
    flush();
    top().lineWidth = width;
}

// 0x08103340: intersects with the current clip rectangle.
void clipTo(int x0, int y0, int x1, int y1)
{
    flush();
    State& s = top();
    int nx0 = clamp(x0, s.clipX0, s.clipX1);
    int ny0 = clamp(y0, s.clipY0, s.clipY1);
    int nx1 = clamp(x1, s.clipX0, s.clipX1);
    int ny1 = clamp(y1, s.clipY0, s.clipY1);
    s.clipX0 = nx0;
    s.clipY0 = ny0;
    s.clipX1 = nx1;
    s.clipY1 = ny1;
    g_pImmediateMode->setScissorRect(nx0, ny0, nx1, ny1);
}

// 0x081033e0
void resetClip()
{
    flush();
    int width, height;
    g_pImmediateMode->getSize(width, height);
    State& s = top();
    s.clipX0 = 0;
    s.clipY0 = 0;
    s.clipX1 = width;
    s.clipY1 = height;
    g_pImmediateMode->setScissorRect(0, 0, width, height);
}

// 0x08103460
void getClipRect(int* x0, int* y0, int* x1, int* y1)
{
    const State& s = top();
    *x0 = s.clipX0;
    *y0 = s.clipY0;
    *x1 = s.clipX1;
    *y1 = s.clipY1;
}

// 0x081034a0
void setBlendMode(Material::BlendMode mode)
{
    flush();
    top().blendMode = mode;
    g_pImmediateMode->setBlendMode(mode);
}

// 0x08103c80
void beginShape(int type, bool threeDee)
{
    select(type, 0, threeDee);
}
// 0x081038a0
void vertexPosition(float x, float y)
{
    g_pCurrentVertex = allocVertex(1);
    g_pCurrentVertex->pos.set(x, y, 0.0f);
}
// 0x081035e0
void vertexTexcoord(float u, float v)
{
    g_pCurrentVertex->u = u;
    g_pCurrentVertex->v = v;
}
// 0x081035b0
void vertexColor(const Color& color)
{
    g_pCurrentVertex->color = color;
}
// 0x08103600
void endShape()
{
    g_pCurrentVertex = 0;
}

// 0x08103b30
void drawPoint(int x, int y, const Color& color)
{
    select(Shape_Points, 0, false);
    IMVertex* v = allocVertex(1);
    setVertex(*v, (float)x, (float)y, 0.0f, color);
}
// 0x08104960
void drawPoint(const Vec2& p, const Color& color)
{
    select(Shape_Points, 0, false);
    IMVertex* v = allocVertex(1);
    setVertex(*v, p.x, p.y, 0.0f, color);
}
// 0x081048d0 (the original also batches these as 2D points)
void drawPoint(const Vec3& p, const Color& color)
{
    select(Shape_Points, 0, false);
    IMVertex* v = allocVertex(1);
    setVertex(*v, p.x, p.y, p.z, color);
}

// 0x081072f0
void drawLine(int x0, int y0, int x1, int y1, const Color& color)
{
    select(Shape_Lines, 0, false);
    IMVertex* v = allocVertex(2);
    setVertex(v[0], (float)x0, (float)y0, 0.0f, color);
    setVertex(v[1], (float)x1, (float)y1, 0.0f, color);
}
// 0x08104800
void drawLine(const Vec2& a, const Vec2& b, const Color& color)
{
    select(Shape_Lines, 0, false);
    IMVertex* v = allocVertex(2);
    setVertex(v[0], a.x, a.y, 0.0f, color);
    setVertex(v[1], b.x, b.y, 0.0f, color);
}
// 0x08104660
void drawLine(const Vec3& a, const Vec3& b, const Color& color)
{
    select(Shape_Lines, 0, true);
    IMVertex* v = allocVertex(2);
    setVertex(v[0], a.x, a.y, a.z, color);
    setVertex(v[1], b.x, b.y, b.z, color);
}
// 0x081059a0
void drawLineAA(int x0, int y0, int x1, int y1, const Color& color)
{
    select(Shape_LinesAA, 0, false);
    IMVertex* v = allocVertex(2);
    setVertex(v[0], (float)x0, (float)y0, 0.0f, color);
    setVertex(v[1], (float)x1, (float)y1, 0.0f, color);
}
// 0x08104730
void drawLineAA(const Vec2& a, const Vec2& b, const Color& color)
{
    select(Shape_LinesAA, 0, false);
    IMVertex* v = allocVertex(2);
    setVertex(v[0], a.x, a.y, 0.0f, color);
    setVertex(v[1], b.x, b.y, 0.0f, color);
}

static void polyLine(int shape, const Vec2* points, int count, bool closed, const Color& color)
{
    select(shape, 0, false);
    int segments = closed ? count : count - 1;
    IMVertex* v = allocVertex(segments * 2);
    for (int i = 0; i < segments; ++i)
    {
        const Vec2& a = points[i];
        const Vec2& b = points[(i + 1) % count];
        setVertex(v[i * 2], a.x, a.y, 0.0f, color);
        setVertex(v[i * 2 + 1], b.x, b.y, 0.0f, color);
    }
}
// 0x08104550
void drawPolyLine(const Vec2* points, int count, bool closed, const Color& color)
{
    polyLine(Shape_Lines, points, count, closed, color);
}
// 0x08104440
void drawPolyLineAA(const Vec2* points, int count, bool closed, const Color& color)
{
    polyLine(Shape_LinesAA, points, count, closed, color);
}

// 0x081076f0
void drawRect(int x, int y, int width, int height, const Color& color)
{
    drawRect(Vec2((float)x, (float)y), Vec2((float)width, (float)height), color);
}
// 0x081073c0: four separate lines.
void drawRect(const Vec2& pos, const Vec2& size, const Color& color)
{
    select(Shape_Lines, 0, false);
    IMVertex* v = allocVertex(8);
    float x0 = pos.x, y0 = pos.y, x1 = pos.x + size.x, y1 = pos.y + size.y;
    setVertex(v[0], x0, y0, 0.0f, color);
    setVertex(v[1], x1, y0, 0.0f, color);
    setVertex(v[2], x1, y0, 0.0f, color);
    setVertex(v[3], x1, y1, 0.0f, color);
    setVertex(v[4], x1, y1, 0.0f, color);
    setVertex(v[5], x0, y1, 0.0f, color);
    setVertex(v[6], x0, y1, 0.0f, color);
    setVertex(v[7], x0, y0, 0.0f, color);
}
// 0x081060c0
void fillRect(int x, int y, int width, int height, const Color& color)
{
    fillRect(Vec2((float)x, (float)y), Vec2((float)width, (float)height), color);
}
// 0x08103e10
void fillRect(const Vec2& pos, const Vec2& size, const Color& color)
{
    select(Shape_Rects, 0, false);
    IMVertex* v = allocVertex(4);
    setVertex(v[0], pos.x, pos.y, 0.0f, color);
    setVertex(v[1], pos.x + size.x, pos.y, 0.0f, color);
    setVertex(v[2], pos.x + size.x, pos.y + size.y, 0.0f, color);
    setVertex(v[3], pos.x, pos.y + size.y, 0.0f, color);
}

// 0x08105430
void drawRoundedRect(int x, int y, int width, int height, float radius, const Color& color)
{
    Vec2 points[RoundedRectPoints];
    roundedRectPoints(x, y, width, height, radius, points);
    polyLine(Shape_Lines, points, RoundedRectPoints, true, color);
}
// 0x08105a70
void fillRoundedRect(int x, int y, int width, int height, float radius, const Color& color)
{
    Vec2 points[RoundedRectPoints];
    roundedRectPoints(x, y, width, height, radius, points);
    fillPolygon(points, RoundedRectPoints, color);
}
// 0x08106000: uses the backend when it can draw rounded rectangles natively.
void fillRoundedRectAA(int x, int y, int width, int height, float radius, const Color& color)
{
    if (!(g_pImmediateMode->getCaps() & ImmediateMode::Caps_RoundedRect))
    {
        flush();
        fillRoundedRect(x, y, width, height, radius, color);
        return;
    }
    flush();
    g_pImmediateMode->drawRoundedRect(Vec2((float)x, (float)y), Vec2((float)width, (float)height),
                                      radius, color);
}

// 0x08106400
void drawEllipse(int x0, int y0, int x1, int y1, int segments, const Color& color)
{
    drawEllipse(Vec2((float)x0, (float)y0), Vec2((float)x1, (float)y1), segments, color);
}
// 0x08106200
void drawEllipse(const Vec2& mn, const Vec2& mx, int segments, const Color& color)
{
    Vec2 center = (mn + mx) * 0.5f;
    Vec2 radius = (mx - mn) * 0.5f;
    float prevS = 0.0f, prevC = 1.0f;
    for (int i = 1; i <= segments; ++i)
    {
        float angle = (float)(i * 2) * PI / (float)segments;
        float sinA = std::sin(angle), cosA = std::cos(angle);
        select(Shape_Lines, 0, false);
        IMVertex* v = allocVertex(2);
        setVertex(v[0], radius.x * prevC + center.x, radius.y * prevS + center.y, 0.0f, color);
        setVertex(v[1], radius.x * cosA + center.x, radius.y * sinA + center.y, 0.0f, color);
        prevS = sinA;
        prevC = cosA;
    }
}

// 0x08103ce0: triangle fan.
void fillPolygon(const Vec2* points, int count, const Color& color)
{
    select(Shape_Triangles, 0, false);
    IMVertex* v = allocVertex(count * 3 - 6);
    for (int i = 1; i < count - 1; ++i)
    {
        setVertex(v[0], points[0].x, points[0].y, 0.0f, color);
        setVertex(v[1], points[i].x, points[i].y, 0.0f, color);
        setVertex(v[2], points[i + 1].x, points[i + 1].y, 0.0f, color);
        v += 3;
    }
}

// 0x081049f0: fan plus a one pixel feathered rim with alpha fading to zero.
void fillPolygonAA(const Vec2* points, int count, const Color& color)
{
    static Vec2 tempCoords[100];
    static Vec2 tempNormals[100];
    if (count > 100)
        count = 100;
    // edge normals, stored on the edge's start point
    for (int i = 0, prev = count - 1; i < count; prev = i++)
    {
        Vec2 edge = points[i] - points[prev];
        float len = std::sqrt(edge.x * edge.x + edge.y * edge.y);
        if (len > 0.0f)
            edge = edge * (1.0f / len);
        tempNormals[prev].set(edge.y, -edge.x);
    }
    // vertex normals: averaged and scaled to keep a one pixel rim at sharp corners
    for (int i = 0, prev = count - 1; i < count; prev = i++)
    {
        Vec2 normal = (tempNormals[prev] + tempNormals[i]) * 0.5f;
        float sqrLength = normal.x * normal.x + normal.y * normal.y;
        if (sqrLength > 1e-6f)
        {
            float scale = 1.0f / sqrLength;
            if (scale > MaxOutlineNormalScale)
                scale = MaxOutlineNormalScale;
            normal = normal * scale;
        }
        tempCoords[i] = points[i] + normal;
    }
    select(Shape_Triangles, 0, false);
    IMVertex* v = allocVertex(count * 9 - 6);
    Color rim(color.r, color.g, color.b, 0);
    for (int i = 0, prev = count - 1; i < count; prev = i++)
    {
        setVertex(v[0], points[i].x, points[i].y, 0.0f, color);
        setVertex(v[1], points[prev].x, points[prev].y, 0.0f, color);
        setVertex(v[2], tempCoords[prev].x, tempCoords[prev].y, 0.0f, rim);
        setVertex(v[3], tempCoords[prev].x, tempCoords[prev].y, 0.0f, rim);
        setVertex(v[4], tempCoords[i].x, tempCoords[i].y, 0.0f, rim);
        setVertex(v[5], points[i].x, points[i].y, 0.0f, color);
        v += 6;
    }
    for (int i = 1; i < count - 1; ++i)
    {
        setVertex(v[0], points[0].x, points[0].y, 0.0f, color);
        setVertex(v[1], points[i].x, points[i].y, 0.0f, color);
        setVertex(v[2], points[i + 1].x, points[i + 1].y, 0.0f, color);
        v += 3;
    }
}

// 0x08103f50: flags = rotation (bits 0-1) | FlipX | FlipY.
void drawImage(RenderableTexture& texture, const Vec2& pos, const Vec2& size, const Vec2& uv0,
               const Vec2& uv1, const Color& color, int flags)
{
    select(Shape_Rects, &texture, false);
    int rotation = flags & 3;
    bool flipX = (flags & FlipX) != 0;
    bool flipY = (flags & FlipY) != 0;
    if (rotation == Rotate180 || rotation == Rotate270)
    {
        flipX = !flipX;
        flipY = !flipY;
    }
    Vec2 p0, p1, p2, p3;
    if (rotation == Rotate90 || rotation == Rotate270)
    {
        // rotated quads swap the extents
        p0.set(pos.x + size.y, pos.y);
        p1.set(pos.x + size.y, pos.y + size.x);
        p2.set(pos.x, pos.y + size.x);
        p3 = pos;
    }
    else
    {
        p0 = pos;
        p1.set(pos.x + size.x, pos.y);
        p2.set(pos.x + size.x, pos.y + size.y);
        p3.set(pos.x, pos.y + size.y);
    }
    float u0 = uv0.x, u1 = uv1.x, v0 = uv0.y, v1 = uv1.y;
    if (flipX)
        swap(u0, u1);
    if (flipY)
        swap(v0, v1);
    IMVertex* v = allocVertex(4);
    setVertex(v[0], p0.x, p0.y, 0.0f, color);
    v[0].u = u0;
    v[0].v = v0;
    setVertex(v[1], p1.x, p1.y, 0.0f, color);
    v[1].u = u1;
    v[1].v = v0;
    setVertex(v[2], p2.x, p2.y, 0.0f, color);
    v[2].u = u1;
    v[2].v = v1;
    setVertex(v[3], p3.x, p3.y, 0.0f, color);
    v[3].u = u0;
    v[3].v = v1;
}
// 0x081041d0
void drawImage(RenderableTexture& texture, const Vec2& pos, const Vec2& size, const Color& color,
               int flags)
{
    drawImage(texture, pos, size, Vec2(0, 0), Vec2(1, 1), color, flags);
}
// 0x08104220
void drawImage(RenderableTexture& texture, int x, int y, int srcX, int srcY, int srcWidth,
               int srcHeight, int width, int height, const Color& color, int flags)
{
    float texWidth = (float)texture.getWidth(), texHeight = (float)texture.getHeight();
    drawImage(texture, Vec2((float)x, (float)y), Vec2((float)width, (float)height),
              Vec2(srcX / texWidth, srcY / texHeight),
              Vec2((srcX + srcWidth) / texWidth, (srcY + srcHeight) / texHeight), color, flags);
}
// 0x081042e0
void drawImage(RenderableTexture& texture, int x, int y, int srcX, int srcY, int srcWidth,
               int srcHeight, const Color& color, int flags)
{
    float texWidth = (float)texture.getWidth(), texHeight = (float)texture.getHeight();
    drawImage(texture, Vec2((float)x, (float)y), Vec2((float)srcWidth, (float)srcHeight),
              Vec2(srcX / texWidth, srcY / texHeight),
              Vec2((srcX + srcWidth) / texWidth, (srcY + srcHeight) / texHeight), color, flags);
}
// 0x081043b0
void drawImage(RenderableTexture& texture, int x, int y, const Color& color, int flags)
{
    drawImage(texture, Vec2((float)x, (float)y),
              Vec2((float)texture.getWidth(), (float)texture.getHeight()), Vec2(0, 0), Vec2(1, 1),
              color, flags);
}

// 0x08104e90: one quad per printable glyph, '\n' moves to the next line.
void drawText(const char* text, const Vec2& pos, Font* font, const Color& color, int maxChars)
{
    float startX = pos.x;
    float y = (float)lrintf(pos.y);
    select(Shape_Rects, font->getTexture(), false);
    int len = (int)strlen(text);
    if (len <= maxChars)
        maxChars = len;
    IMVertex* v = allocVertex(maxChars * 4);
    int lineHeight = font->getLineHeight();
    int drawn = 0;
    float x = (float)lrintf(startX);
    for (int i = 0; i < maxChars; ++i)
    {
        unsigned char ch = (unsigned char)text[i];
        const Font::Glyph* glyph = font->getGlyph(ch);
        if (ch == '\n')
        {
            y += (float)lineHeight;
            x = (float)lrintf(startX);
        }
        if (glyph->width != 0)
        {
            float x0 = x + glyph->xoffset, y0 = y + glyph->yoffset;
            float x1 = x0 + glyph->width, y1 = y0 + glyph->height;
            setVertex(v[0], x0, y0, 0.0f, color);
            v[0].u = glyph->u0;
            v[0].v = glyph->v0;
            setVertex(v[1], x1, y0, 0.0f, color);
            v[1].u = glyph->u1;
            v[1].v = glyph->v0;
            setVertex(v[2], x1, y1, 0.0f, color);
            v[2].u = glyph->u1;
            v[2].v = glyph->v1;
            setVertex(v[3], x0, y1, 0.0f, color);
            v[3].u = glyph->u0;
            v[3].v = glyph->v1;
            v += 4;
            ++drawn;
        }
        x += (float)glyph->advance;
    }
    g_nextVertex += (drawn - maxChars) * 4;
}
// 0x08105140
void drawText(const char* text, int x, int y, Font* font, const Color& color, int maxChars)
{
    drawText(text, Vec2((float)x, (float)y), font, color, maxChars);
}

// 0x08106440
void flushDebugDraw(const Matrix4x4& viewProj, Font* font)
{
    int width, height;
    g_pImmediateMode->getSize(width, height);
    flush();
    top().transform = viewProj;
    beginDraw();
    for (int i = 0; i < DebugDraw::sm_points.size(); ++i)
        drawPoint(DebugDraw::sm_points[i].pos, DebugDraw::sm_points[i].color);
    for (int i = 0; i < DebugDraw::sm_lines.size(); ++i)
        drawLine(DebugDraw::sm_lines[i].a, DebugDraw::sm_lines[i].b, DebugDraw::sm_lines[i].color);
    endDraw();

    setIdentityTransform();
    beginDraw();
    for (int i = 0; i < DebugDraw::sm_lines2d.size(); ++i)
        drawLine(DebugDraw::sm_lines2d[i].a, DebugDraw::sm_lines2d[i].b,
                 DebugDraw::sm_lines2d[i].color);
    for (int i = 0; i < DebugDraw::sm_texts.size(); ++i)
    {
        const DebugDraw::Text& entry = DebugDraw::sm_texts[i];
        drawText(entry.text.c_str(), Vec2(entry.pos.x, entry.pos.y), font, entry.color, INT_MAX);
    }
    for (int i = 0; i < DebugDraw::sm_texts3d.size(); ++i)
    {
        const DebugDraw::Text& entry = DebugDraw::sm_texts3d[i];
        const float* m = viewProj.m;
        const Vec3& pos = entry.pos;
        float w = m[3] * pos.x + m[7] * pos.y + m[11] * pos.z + m[15];
        if (w > 0.0f)
        {
            float invW = 1.0f / w;
            float clipX = (m[0] * pos.x + m[4] * pos.y + m[8] * pos.z + m[12]) * invW;
            float clipY = (m[1] * pos.x + m[5] * pos.y + m[9] * pos.z + m[13]) * invW;
            Vec2 screenPos((float)(int)(width * (clipX * 0.5f + 0.5f)),
                           (float)(int)(height * (clipY * -0.5f + 0.5f)));
            drawText(entry.text.c_str(), screenPos, font, entry.color, INT_MAX);
        }
    }
    endDraw();

    DebugDraw::sm_points.clear();
    DebugDraw::sm_lines.clear();
    DebugDraw::sm_lines2d.clear();
    DebugDraw::sm_texts.clear();
    DebugDraw::sm_texts3d.clear();
}

} // namespace im
} // namespace engine
