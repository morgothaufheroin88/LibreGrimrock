// Reconstructed from Grimrock.bin.x86 Noise.cpp. Ken Perlin's reference permutation
// (512 entries at 0x08235ca0).
//
// The lattice cell of a coordinate is a plain truncating (int) cast in both games: the
// i386 build switches the x87 control word to truncation around its FISTL (Ghidra shows
// that as ROUND()), grimrock2.exe calls __ftol2_sse (0x00452c90, CVTTSD2SI). Rounding to
// nearest instead turns the fraction of a coordinate past .5 negative, fade() then
// extrapolates (at -0.5 the weight is -2.375) and the field breaks at every half integer:
// that was the source of the pits and the small pyramids in the terrain of the second game.
#include "core/Noise.h"
#include <cmath>

namespace core
{

static constexpr int p[512] = {
    151, 160, 137, 91,  90,  15,  131, 13,  201, 95,  96,  53,  194, 233, 7,   225, 140, 36,  103,
    30,  69,  142, 8,   99,  37,  240, 21,  10,  23,  190, 6,   148, 247, 120, 234, 75,  0,   26,
    197, 62,  94,  252, 219, 203, 117, 35,  11,  32,  57,  177, 33,  88,  237, 149, 56,  87,  174,
    20,  125, 136, 171, 168, 68,  175, 74,  165, 71,  134, 139, 48,  27,  166, 77,  146, 158, 231,
    83,  111, 229, 122, 60,  211, 133, 230, 220, 105, 92,  41,  55,  46,  245, 40,  244, 102, 143,
    54,  65,  25,  63,  161, 1,   216, 80,  73,  209, 76,  132, 187, 208, 89,  18,  169, 200, 196,
    135, 130, 116, 188, 159, 86,  164, 100, 109, 198, 173, 186, 3,   64,  52,  217, 226, 250, 124,
    123, 5,   202, 38,  147, 118, 126, 255, 82,  85,  212, 207, 206, 59,  227, 47,  16,  58,  17,
    182, 189, 28,  42,  223, 183, 170, 213, 119, 248, 152, 2,   44,  154, 163, 70,  221, 153, 101,
    155, 167, 43,  172, 9,   129, 22,  39,  253, 19,  98,  108, 110, 79,  113, 224, 232, 178, 185,
    112, 104, 218, 246, 97,  228, 251, 34,  242, 193, 238, 210, 144, 12,  191, 179, 162, 241, 81,
    51,  145, 235, 249, 14,  239, 107, 49,  192, 214, 31,  181, 199, 106, 157, 184, 84,  204, 176,
    115, 121, 50,  45,  127, 4,   150, 254, 138, 236, 205, 93,  222, 114, 67,  29,  24,  72,  243,
    141, 128, 195, 78,  66,  215, 61,  156, 180, 151, 160, 137, 91,  90,  15,  131, 13,  201, 95,
    96,  53,  194, 233, 7,   225, 140, 36,  103, 30,  69,  142, 8,   99,  37,  240, 21,  10,  23,
    190, 6,   148, 247, 120, 234, 75,  0,   26,  197, 62,  94,  252, 219, 203, 117, 35,  11,  32,
    57,  177, 33,  88,  237, 149, 56,  87,  174, 20,  125, 136, 171, 168, 68,  175, 74,  165, 71,
    134, 139, 48,  27,  166, 77,  146, 158, 231, 83,  111, 229, 122, 60,  211, 133, 230, 220, 105,
    92,  41,  55,  46,  245, 40,  244, 102, 143, 54,  65,  25,  63,  161, 1,   216, 80,  73,  209,
    76,  132, 187, 208, 89,  18,  169, 200, 196, 135, 130, 116, 188, 159, 86,  164, 100, 109, 198,
    173, 186, 3,   64,  52,  217, 226, 250, 124, 123, 5,   202, 38,  147, 118, 126, 255, 82,  85,
    212, 207, 206, 59,  227, 47,  16,  58,  17,  182, 189, 28,  42,  223, 183, 170, 213, 119, 248,
    152, 2,   44,  154, 163, 70,  221, 153, 101, 155, 167, 43,  172, 9,   129, 22,  39,  253, 19,
    98,  108, 110, 79,  113, 224, 232, 178, 185, 112, 104, 218, 246, 97,  228, 251, 34,  242, 193,
    238, 210, 144, 12,  191, 179, 162, 241, 81,  51,  145, 235, 249, 14,  239, 107, 49,  192, 214,
    31,  181, 199, 106, 157, 184, 84,  204, 176, 115, 121, 50,  45,  127, 4,   150, 254, 138, 236,
    205, 93,  222, 114, 67,  29,  24,  72,  243, 141, 128, 195, 78,  66,  215, 61,  156, 180};

static inline int cell(float value)
{
    return (int)value;
}
static inline double fade(double t)
{
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}
static inline double lerpd(double t, double a, double b)
{
    return a + t * (b - a);
}
static inline double grad(int hash, double x, double y, double z)
{
    int h = hash & 15;
    double u = h < 8 ? x : y;
    double v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

// 0x080b2d60
float noise(float xf)
{
    int xi = cell(xf);
    double x = (double)xf - xi;
    int X = xi & 255;
    double u = fade(x);
    int A = p[X], B = p[X + 1];
    return (float)lerpd(u, grad(p[p[A]], x, 0, 0), grad(p[p[B]], x - 1, 0, 0));
}
// 0x080b2e90
float noise(float xf, float yf)
{
    int xi = cell(xf), yi = cell(yf);
    double x = (double)xf - xi, y = (double)yf - yi;
    int X = xi & 255, Y = yi & 255;
    double u = fade(x), v = fade(y);
    int A = p[X] + Y, AA = p[A], AB = p[A + 1];
    int B = p[X + 1] + Y, BA = p[B], BB = p[B + 1];
    return (float)lerpd(v, lerpd(u, grad(p[AA], x, y, 0), grad(p[BA], x - 1, y, 0)),
                        lerpd(u, grad(p[AB], x, y - 1, 0), grad(p[BB], x - 1, y - 1, 0)));
}
// 0x080b3140
float noise(float xf, float yf, float zf)
{
    int xi = cell(xf), yi = cell(yf), zi = cell(zf);
    double x = (double)xf - xi, y = (double)yf - yi, z = (double)zf - zi;
    int X = xi & 255, Y = yi & 255, Z = zi & 255;
    double u = fade(x), v = fade(y), w = fade(z);
    int A = p[X] + Y, AA = p[A] + Z, AB = p[A + 1] + Z;
    int B = p[X + 1] + Y, BA = p[B] + Z, BB = p[B + 1] + Z;
    return (float)lerpd(
        w,
        lerpd(v, lerpd(u, grad(p[AA], x, y, z), grad(p[BA], x - 1, y, z)),
              lerpd(u, grad(p[AB], x, y - 1, z), grad(p[BB], x - 1, y - 1, z))),
        lerpd(v, lerpd(u, grad(p[AA + 1], x, y, z - 1), grad(p[BA + 1], x - 1, y, z - 1)),
              lerpd(u, grad(p[AB + 1], x, y - 1, z - 1), grad(p[BB + 1], x - 1, y - 1, z - 1))));
}

} // namespace core
