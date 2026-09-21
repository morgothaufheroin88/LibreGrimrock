// Reconstructed from Grimrock.bin.x86 DebugDraw.cpp.
#include "core/DebugDraw.h"
#include "core/Math.h"
#include <cmath>

namespace core
{

Array<DebugDraw::Point> DebugDraw::sm_points;
Array<DebugDraw::Line> DebugDraw::sm_lines;
Array<DebugDraw::Line2d> DebugDraw::sm_lines2d;
Array<DebugDraw::Text> DebugDraw::sm_texts;
Array<DebugDraw::Text> DebugDraw::sm_texts3d;

// 0x080bc860
void DebugDraw::drawPoint(const Vec3& p, const Color& color)
{
    Point& pt = sm_points.push_back();
    pt.pos = p;
    pt.color = color;
}
// 0x080bc590
void DebugDraw::drawLine(const Vec3& a, const Vec3& b, const Color& color)
{
    Line& l = sm_lines.push_back();
    l.a = a;
    l.b = b;
    l.color = color;
}
// 0x080bc700
void DebugDraw::drawLine(const Vec2& a, const Vec2& b, const Color& color)
{
    Line2d& l = sm_lines2d.push_back();
    l.a = a;
    l.b = b;
    l.color = color;
}
// 0x080bc460
void DebugDraw::drawText(const char* text, const Vec2& pos, const Color& color)
{
    Text entry;
    entry.text = text;
    entry.pos.set(pos.x, pos.y, 1.0f);
    entry.color = color;
    sm_texts.push_back(entry);
}
// 0x080bcb00
void DebugDraw::drawText(const char* text, const Vec3& pos, const Color& color)
{
    Text entry;
    entry.text = text;
    entry.pos = pos;
    entry.color = color;
    sm_texts3d.push_back(entry);
}
// 0x080bcc30
void DebugDraw::drawBox(const Vec2& mn, const Vec2& mx, const Color& color)
{
    drawLine(Vec2(mn.x, mn.y), Vec2(mx.x, mn.y), color);
    drawLine(Vec2(mx.x, mn.y), Vec2(mx.x, mx.y), color);
    drawLine(Vec2(mx.x, mx.y), Vec2(mn.x, mx.y), color);
    drawLine(Vec2(mn.x, mx.y), Vec2(mn.x, mn.y), color);
}
// 0x080bd190
void DebugDraw::drawBox(const AABox2& box, const Color& color)
{
    drawBox(box.min, box.max, color);
}
// 0x080c1d60: 12 edges of the box transformed by m.
void DebugDraw::drawBox(const Vec3& mn, const Vec3& mx, const Matrix4x3& m, const Color& color)
{
    Vec3 corners[8];
    for (int i = 0; i < 8; ++i)
        corners[i] =
            m.transformPoint(Vec3(i & 1 ? mx.x : mn.x, i & 2 ? mx.y : mn.y, i & 4 ? mx.z : mn.z));
    static constexpr int edges[12][2] = {{0, 1}, {1, 3}, {3, 2}, {2, 0}, {4, 5}, {5, 7},
                                         {7, 6}, {6, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (int i = 0; i < 12; ++i)
        drawLine(corners[edges[i][0]], corners[edges[i][1]], color);
}
// 0x080c3f70
void DebugDraw::drawBox(const AABox3& box, const Matrix4x3& m, const Color& color)
{
    drawBox(box.min, box.max, m, color);
}
// 0x080c3fa0
void DebugDraw::drawBox(const AABox3& box, const Color& color)
{
    drawBox(box.min, box.max, Matrix4x3::sm_mIdentity, color);
}
// 0x080c3fd0
void DebugDraw::drawBox(const Vec3& mn, const Vec3& mx, const Color& color)
{
    drawBox(mn, mx, Matrix4x3::sm_mIdentity, color);
}
// 0x080bd6f0
void DebugDraw::drawSphere(const Vec3& center, float radius, const Color& color, int segments)
{
    drawSphere(center, radius, Matrix4x3::sm_mIdentity, color, segments);
}
// 0x080be120: latitude rings and longitude arcs.
void DebugDraw::drawSphere(const Vec3& center, float radius, const Matrix4x3& m, const Color& color,
                           int segments)
{
    int ringSegments = segments * 2;
    float theta = 0.0f;
    for (int i = 0; i < segments; ++i)
    {
        float sinTheta = std::sin(theta), cosTheta = std::cos(theta);
        float phi = 0.0f;
        Vec3 prev = m.transformPoint(
            center + Vec3(0.0f * radius * sinTheta, -cosTheta * radius, radius * sinTheta));
        for (int j = 0; j < ringSegments; ++j)
        {
            phi += TWO_PI / (float)ringSegments;
            float sinPhi = std::sin(phi), cosPhi = std::cos(phi);
            Vec3 point =
                m.transformPoint(center + Vec3(cosPhi * radius * sinTheta, -cosTheta * radius,
                                               sinPhi * radius * sinTheta));
            drawLine(prev, point, color);
            prev = point;
        }
        theta += PI / (float)segments;
    }
    float phi = 0.0f;
    for (int j = 0; j < ringSegments; ++j)
    {
        float sinPhi = std::sin(phi), cosPhi = std::cos(phi);
        float arcAngle = 0.0f;
        Vec3 prev = m.transformPoint(center + Vec3(0.0f, -radius, 0.0f));
        for (int i = 0; i < segments; ++i)
        {
            arcAngle += PI / (float)segments;
            float sinArc = std::sin(arcAngle), cosArc = std::cos(arcAngle);
            Vec3 point = m.transformPoint(center + Vec3(cosPhi * radius * sinArc, -cosArc * radius,
                                                        sinPhi * radius * sinArc));
            drawLine(prev, point, color);
            prev = point;
        }
        phi += TWO_PI / (float)ringSegments;
    }
}
// 0x080bead0: base ring at the origin and lines to the apex along +z.
void DebugDraw::drawCone(float radius, float height, const Matrix4x3& m, const Color& color,
                         int segments)
{
    Vec3 apex = m.transformPoint(Vec3(0, 0, height));
    Vec3 prev = m.transformPoint(Vec3(radius, 0, 0));
    for (int i = 1; i <= segments; ++i)
    {
        float angle = (float)i * TWO_PI / (float)segments;
        Vec3 point = m.transformPoint(Vec3(std::cos(angle) * radius, std::sin(angle) * radius, 0));
        drawLine(prev, point, color);
        drawLine(point, apex, color);
        prev = point;
    }
}
// 0x080bf150: two hemispheres joined by four lines along the local y axis.
void DebugDraw::drawCapsule(float radius, float height, const Matrix4x3& m, const Color& color)
{
    float halfHeight = height * 0.5f;
    constexpr int ArcSegments = 8; // per quarter circle
    // side lines
    drawLine(m.transformPoint(Vec3(radius, -halfHeight, 0)),
             m.transformPoint(Vec3(radius, halfHeight, 0)), color);
    drawLine(m.transformPoint(Vec3(-radius, -halfHeight, 0)),
             m.transformPoint(Vec3(-radius, halfHeight, 0)), color);
    drawLine(m.transformPoint(Vec3(0, -halfHeight, radius)),
             m.transformPoint(Vec3(0, halfHeight, radius)), color);
    drawLine(m.transformPoint(Vec3(0, -halfHeight, -radius)),
             m.transformPoint(Vec3(0, halfHeight, -radius)), color);
    // rings at both ends
    for (int end = 0; end < 2; ++end)
    {
        float y = end ? halfHeight : -halfHeight;
        Vec3 prev = m.transformPoint(Vec3(radius, y, 0));
        for (int i = 1; i <= ArcSegments * 2; ++i)
        {
            float angle = (float)i * TWO_PI / (float)(ArcSegments * 2);
            Vec3 point =
                m.transformPoint(Vec3(std::cos(angle) * radius, y, std::sin(angle) * radius));
            drawLine(prev, point, color);
            prev = point;
        }
    }
    // hemisphere arcs in the xy and zy planes
    for (int plane = 0; plane < 2; ++plane)
    {
        for (int end = 0; end < 2; ++end)
        {
            float y = end ? halfHeight : -halfHeight;
            float sign = end ? 1.0f : -1.0f;
            Vec3 prev;
            for (int i = 0; i <= ArcSegments; ++i)
            {
                float angle = (float)i * HALF_PI / (float)ArcSegments;
                float cosA = std::cos(angle), sinA = std::sin(angle);
                Vec3 local;
                for (int side = -1; side <= 1; side += 2)
                {
                    if (plane == 0)
                        local = Vec3((float)side * cosA * radius, y + sign * sinA * radius, 0);
                    else
                        local = Vec3(0, y + sign * sinA * radius, (float)side * cosA * radius);
                    Vec3 point = m.transformPoint(local);
                    if (i > 0)
                        drawLine(prev, point, color);
                    prev = point;
                }
            }
        }
    }
}
// 0x080c0390: wire camera model; the point table is CameraModelWidth units wide.
void DebugDraw::drawCamera(const Matrix4x3& m, float size, const Color& color)
{
    constexpr float CameraModelWidth = 50.8f;
    constexpr int NumCameraLines = 33;
    static constexpr int indices[NumCameraLines * 2] = {
        0,  1,  1,  2,  2,  3,  3,  0,  4,  5,  5,  6,  6,  7,  7,  4,  0,  4,  1,  5,  2,  6,
        3,  7,  8,  9,  10, 11, 11, 8,  12, 13, 14, 15, 15, 12, 8,  12, 11, 15, 9,  13, 10, 14,
        16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 24, 25, 25, 26, 16, 26};
    static constexpr float points[27][3] = {
        {10.9f, 20.2f, -25.4f},  {10.9f, 20.2f, 25.4f},    {10.9f, -20.2f, 25.4f},
        {10.9f, -20.2f, -25.4f}, {-10.9f, 20.2f, -25.4f},  {-10.9f, 20.2f, 25.4f},
        {-10.9f, -20.2f, 25.4f}, {-10.9f, -20.2f, -25.4f}, {10.9f, -10.3f, 50.7f},
        {10.9f, -5.5f, 25.4f},   {10.9f, 5.5f, 25.4f},     {10.9f, 10.3f, 50.7f},
        {-10.9f, -10.3f, 50.7f}, {-10.9f, -5.5f, 25.4f},   {-10.9f, 5.5f, 25.4f},
        {-10.9f, 10.3f, 50.7f},  {0.0f, 20.2f, 25.4f},     {0.0f, 31.7f, 36.9f},
        {0.0f, 47.9f, 36.9f},    {0.0f, 59.4f, 25.4f},     {0.0f, 59.4f, 11.5f},
        {0.0f, 47.9f, 0.0f},     {0.0f, 59.4f, -11.5f},    {0.0f, 59.4f, -25.4f},
        {0.0f, 47.9f, -36.9f},   {0.0f, 31.7f, -36.9f},    {0.0f, 20.2f, -25.4f}};
    float scale = size / CameraModelWidth;
    for (int i = 0; i < NumCameraLines; ++i)
    {
        const float* a = points[indices[i * 2]];
        const float* b = points[indices[i * 2 + 1]];
        drawLine(m.transformPoint(Vec3(a[0] * scale, a[1] * scale, a[2] * scale)),
                 m.transformPoint(Vec3(b[0] * scale, b[1] * scale, b[2] * scale)), color);
    }
}
// 0x080c0700
void DebugDraw::drawBase(const Matrix4x3& m, float size)
{
    drawLine(m.pos, m.transformPoint(Vec3(size, 0, 0)), Color::Red);
    drawLine(m.pos, m.transformPoint(Vec3(0, size, 0)), Color::Green);
    drawLine(m.pos, m.transformPoint(Vec3(0, 0, size)), Color::Blue);
}
// 0x080c0d20: the three axes of half size plus the four body diagonals at 0.7 of it.
void DebugDraw::drawPointLight(const Matrix4x3& m, float size, const Color& color)
{
    constexpr float DiagonalScale = 0.7f;
    float half = size * 0.5f;
    drawLine(m.transformPoint(Vec3(-half, 0, 0)), m.transformPoint(Vec3(half, 0, 0)), color);
    drawLine(m.transformPoint(Vec3(0, -half, 0)), m.transformPoint(Vec3(0, half, 0)), color);
    drawLine(m.transformPoint(Vec3(0, 0, -half)), m.transformPoint(Vec3(0, 0, half)), color);
    float d = half * DiagonalScale;
    drawLine(m.transformPoint(Vec3(-d, -d, -d)), m.transformPoint(Vec3(d, d, d)), color);
    drawLine(m.transformPoint(Vec3(d, -d, -d)), m.transformPoint(Vec3(-d, d, d)), color);
    drawLine(m.transformPoint(Vec3(-d, -d, d)), m.transformPoint(Vec3(d, d, -d)), color);
    drawLine(m.transformPoint(Vec3(d, -d, d)), m.transformPoint(Vec3(-d, d, -d)), color);
}
// 0x080c1ce0
void DebugDraw::drawPointLight(const Vec3& pos, float size, const Color& color)
{
    Matrix4x3 m;
    m.pos = pos;
    drawPointLight(m, size, color);
}
// 0x080c4000
void DebugDraw::drawFrustum(const Matrix4x3& m, float fov, float aspect, float nearZ, float farZ,
                            const Color& color)
{
    float tanHalfFov = (float)std::tan(fov * 0.5f);
    float nearHeight = tanHalfFov * nearZ, nearWidth = nearHeight * aspect;
    float farHeight = tanHalfFov * farZ, farWidth = farHeight * aspect;
    Vec3 corners[8];
    corners[0] = m.transformPoint(Vec3(-nearWidth, nearHeight, nearZ));
    corners[1] = m.transformPoint(Vec3(nearWidth, nearHeight, nearZ));
    corners[2] = m.transformPoint(Vec3(nearWidth, -nearHeight, nearZ));
    corners[3] = m.transformPoint(Vec3(-nearWidth, -nearHeight, nearZ));
    corners[4] = m.transformPoint(Vec3(-farWidth, farHeight, farZ));
    corners[5] = m.transformPoint(Vec3(farWidth, farHeight, farZ));
    corners[6] = m.transformPoint(Vec3(farWidth, -farHeight, farZ));
    corners[7] = m.transformPoint(Vec3(-farWidth, -farHeight, farZ));
    for (int i = 0; i < 4; ++i)
    {
        drawLine(corners[i], corners[(i + 1) % 4], color);
        drawLine(corners[4 + i], corners[4 + (i + 1) % 4], color);
        drawLine(corners[i], corners[4 + i], color);
    }
}
void DebugDraw::clear()
{
    sm_points.resize(0);
    sm_lines.resize(0);
    sm_lines2d.resize(0);
    sm_texts.resize(0);
    sm_texts3d.resize(0);
}

} // namespace core
