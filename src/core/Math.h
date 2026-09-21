// core::Math.cpp helpers (0x080c6bb0 fsqrt, 0x080c6c40 initMath) plus the inline
// helpers that the rest of the engine used from the math header.
#pragma once
#include "core/Vector.h"
#include <cmath>
#include <cstdlib>

namespace core
{

constexpr float PI = 3.14159265358979f;
constexpr float TWO_PI = 6.28318530717959f;
constexpr float HALF_PI = 1.5707963267949f;

// Table based square root approximation (Math.cpp). initMath() fills the tables.
void initMath();
float fsqrt(float x);

inline float degToRad(float d)
{
    return d * (PI / 180.0f);
}
inline float radToDeg(float r)
{
    return r * (180.0f / PI);
}
template <class T> inline T min(T a, T b)
{
    return a < b ? a : b;
}
template <class T> inline T max(T a, T b)
{
    return a > b ? a : b;
}
template <class T> inline T clamp(T v, T lo, T hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}
template <class T> inline T lerp(T a, T b, float t)
{
    return a + (b - a) * t;
}
template <class T> inline T sqr(T v)
{
    return v * v;
}
template <class T> inline T sign(T v)
{
    return v < 0 ? (T)-1 : (v > 0 ? (T)1 : (T)0);
}
template <class T> inline void swap(T& a, T& b)
{
    T t = a;
    a = b;
    b = t;
}
inline float frac(float v)
{
    return v - std::floor(v);
}
inline int roundToInt(float v)
{
    return (int)std::floor(v + 0.5f);
}
inline bool isPowerOfTwo(int v)
{
    return v > 0 && (v & (v - 1)) == 0;
}
inline int nextPowerOfTwo(int v)
{
    int r = 1;
    while (r < v)
        r <<= 1;
    return r;
}
inline float randomFloat()
{
    return (float)rand() / (float)RAND_MAX;
}
inline float randomFloat(float lo, float hi)
{
    return lo + (hi - lo) * randomFloat();
}

} // namespace core
