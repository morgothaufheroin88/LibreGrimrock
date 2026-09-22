// Reconstructed from Grimrock.bin.x86 Color.cpp.
#include "core/Color.h"
#include <cstring>

namespace core
{

#if GRIMROCK_GAME >= 2
static int hexDigit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return 0;
}
// 0x0049e340
Color Color::fromHex(const char* hex)
{
    Color color(0, 0, 0, 255);
    unsigned char* channels = &color.r;
    int length = (int)strlen(hex);
    for (int i = 0; i < 4 && i * 2 + 1 < length; ++i)
        channels[i] = (unsigned char)((hexDigit(hex[i * 2]) << 4) | hexDigit(hex[i * 2 + 1]));
    return color;
}
#endif

const Color Color::Black(0, 0, 0, 255);
const Color Color::White(255, 255, 255, 255);
const Color Color::Red(255, 0, 0, 255);
const Color Color::Green(0, 255, 0, 255);
const Color Color::Blue(0, 0, 255, 255);
const Color Color::Cyan(0, 255, 255, 255);
const Color Color::Magenta(255, 0, 255, 255);
const Color Color::Yellow(255, 255, 0, 255);

} // namespace core
