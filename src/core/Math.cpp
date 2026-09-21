// Reconstructed from Grimrock.bin.x86 Math.cpp.
#include "core/Math.h"
#include <cmath>
#include <cstdint>
#include <cstring>

namespace core
{

// IEEE single precision layout used by the table lookup.
constexpr uint32_t FloatMantissaMask = 0x7fffff;
constexpr uint32_t FloatImplicitOne = 0x800000;
constexpr int FloatMantissaBits = 23;
constexpr int FloatExponentBias = 0x7f;
constexpr uint32_t FloatExponentMask = 0x7f;
constexpr uint32_t FloatOneBits = 0x3f800000u; // 1.0f
constexpr uint32_t FloatTwoBits = 0x40000000u; // 2.0f

// Two 128-entry tables of the upper mantissa bits of sqrt(1.m) and sqrt(2.m).
constexpr int SqrtTableHalf = 128;
static unsigned short sqrttab[SqrtTableHalf * 2];

// 0x080c6c40 core::initMath()
void initMath()
{
    for (int i = 0; i < SqrtTableHalf; ++i)
    {
        union
        {
            uint32_t bits;
            float value;
        } entry;
        // mantissa i in [1,2): sqrt of the even exponent half of the table
        entry.bits = ((uint32_t)i << 16) | FloatOneBits;
        float root = std::sqrt(entry.value);
        uint32_t rootBits;
        std::memcpy(&rootBits, &root, 4);
        sqrttab[i] = (unsigned short)((rootBits >> 16) & FloatExponentMask);
        // mantissa i in [2,4): the odd exponent half
        entry.bits = ((uint32_t)i << 16) | FloatTwoBits;
        root = std::sqrt(entry.value);
        std::memcpy(&rootBits, &root, 4);
        sqrttab[SqrtTableHalf + i] = (unsigned short)((rootBits >> 16) & FloatExponentMask);
    }
}

// 0x080c6bb0 core::fsqrt(float)
float fsqrt(float x)
{
    if (x == 0.0f)
        return 0.0f;
    uint32_t bits;
    std::memcpy(&bits, &x, 4);
    uint32_t mantissa = bits & FloatMantissaMask;
    int exponent = (int)(short)((unsigned short)(bits >> FloatMantissaBits) - FloatExponentBias);
    uint32_t key = mantissa;
    if (exponent & 1)
        key = mantissa | FloatImplicitOne;
    uint32_t resultBits = (uint32_t)(((exponent >> 1) + FloatExponentBias) * FloatImplicitOne) |
                          ((uint32_t)(short)sqrttab[key >> 16] << 16);
    float result;
    std::memcpy(&result, &resultBits, 4);
    return result;
}

} // namespace core
