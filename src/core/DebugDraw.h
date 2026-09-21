// Reconstructed from Grimrock.bin.x86 DebugDraw.cpp (0x080bc460-0x080c5900).
// Primitives are queued in static arrays and flushed by engine::im::flushDebugDraw().
#pragma once
#include "core/Array.h"
#include "core/Color.h"
#include "core/Matrix.h"
#include "core/Prim.h"
#include "core/String.h"

namespace core
{

class DebugDraw
{
  public:
    struct Point
    {
        Vec3 pos;
        Color color;
    };
    struct Line
    {
        Vec3 a, b;
        Color color;
    };
    struct Line2d
    {
        Vec2 a, b;
        Color color;
    };
    struct Text
    {
        String text;
        Vec3 pos; // 2d texts store z = 1
        Color color;
    };

    static void drawPoint(const Vec3& p, const Color& color);
    static void drawLine(const Vec3& a, const Vec3& b, const Color& color);
    static void drawLine(const Vec2& a, const Vec2& b, const Color& color);
    static void drawText(const char* text, const Vec2& pos, const Color& color);
    static void drawText(const char* text, const Vec3& pos, const Color& color);
    static void drawBox(const Vec2& min, const Vec2& max, const Color& color);
    static void drawBox(const AABox2& box, const Color& color);
    static void drawBox(const Vec3& min, const Vec3& max, const Matrix4x3& m, const Color& color);
    static void drawBox(const AABox3& box, const Matrix4x3& m, const Color& color);
    static void drawBox(const AABox3& box, const Color& color);
    static void drawBox(const Vec3& min, const Vec3& max, const Color& color);
    static void drawSphere(const Vec3& center, float radius, const Color& color, int segments = 8);
    static void drawSphere(const Vec3& center, float radius, const Matrix4x3& m, const Color& color,
                           int segments = 8);
    static void drawCone(float radius, float height, const Matrix4x3& m, const Color& color,
                         int segments = 8);
    static void drawCapsule(float radius, float height, const Matrix4x3& m, const Color& color);
    static void drawCamera(const Matrix4x3& m, float size, const Color& color);
    static void drawBase(const Matrix4x3& m, float size);
    static void drawPointLight(const Matrix4x3& m, float size, const Color& color);
    static void drawPointLight(const Vec3& pos, float size, const Color& color);
    static void drawFrustum(const Matrix4x3& m, float fov, float aspect, float nearZ, float farZ,
                            const Color& color);
    static void clear();

    static Array<Point> sm_points;
    static Array<Line> sm_lines;
    static Array<Line2d> sm_lines2d;
    static Array<Text> sm_texts;
    static Array<Text> sm_texts3d;
};

} // namespace core
