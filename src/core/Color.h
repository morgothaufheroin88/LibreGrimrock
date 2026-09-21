// core::Color, 4 packed bytes (r, g, b, a). Constants from Color.cpp (0x080b3670).
#pragma once
#include "core/Vector.h"

namespace core
{

class Color
{
  public:
    unsigned char r, g, b, a;

    Color() : r(0), g(0), b(0), a(255) {}
    Color(int r_, int g_, int b_, int a_ = 255)
        : r((unsigned char)r_), g((unsigned char)g_), b((unsigned char)b_), a((unsigned char)a_)
    {
    }
    Color(float r_, float g_, float b_, float a_ = 1.0f)
        : r(toByte(r_)), g(toByte(g_)), b(toByte(b_)), a(toByte(a_))
    {
    }
    explicit Color(const Vec4& v) : r(toByte(v.x)), g(toByte(v.y)), b(toByte(v.z)), a(toByte(v.w))
    {
    }
    explicit Color(const Vec3& v) : r(toByte(v.x)), g(toByte(v.y)), b(toByte(v.z)), a(255) {}

    unsigned int toRGBA() const
    {
        return (unsigned int)r | ((unsigned int)g << 8) | ((unsigned int)b << 16) |
               ((unsigned int)a << 24);
    }
    Vec4 toVec4() const
    {
        return Vec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    }
    Vec3 toVec3() const
    {
        return Vec3(r / 255.0f, g / 255.0f, b / 255.0f);
    }
    bool operator==(const Color& o) const
    {
        return r == o.r && g == o.g && b == o.b && a == o.a;
    }
    bool operator!=(const Color& o) const
    {
        return !(*this == o);
    }

    static unsigned char toByte(float v)
    {
        if (v <= 0.0f)
            return 0;
        if (v >= 1.0f)
            return 255;
        return (unsigned char)(v * 255.0f + 0.5f);
    }

    static const Color Black;
    static const Color White;
    static const Color Red;
    static const Color Green;
    static const Color Blue;
    static const Color Cyan;
    static const Color Magenta;
    static const Color Yellow;
};

} // namespace core
