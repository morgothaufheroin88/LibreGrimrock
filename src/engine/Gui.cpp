// Reconstructed from Grimrock.bin.x86 Gui.cpp.
#include "engine/Gui.h"
#include "core/Vector.h"
#include "engine/Font.h"
#include "engine/ImmediateMode.h"
#include <climits>
#include <cmath>

namespace engine
{
namespace gui
{

using namespace core;

// Widget palette (grey levels) and metrics of the debug GUI.
Color backgroundColor(0x33, 0x33, 0x33, 0xff);
Color textColor(0xeb, 0xeb, 0xeb, 0xff);
Color groupColor(0x59, 0x59, 0x59, 0xff);
Color buttonColor(0x59, 0x59, 0x59, 0xff);
Color activeButtonColor(100, 100, 100, 0xff);
Color checkBoxBackgroundColor(0x66, 0x66, 0x66, 0xff);
Color activeCheckBoxBackgroundColor(0x78, 0x78, 0x78, 0xff);
Color checkBoxTickColor(0xeb, 0xeb, 0xeb, 0xff);
Color sliderKnobColor(0x8c, 0x8c, 0x8c, 0xff);
Color activeSliderKnobColor(0xc8, 0xc8, 0xc8, 0xff);
Color scrollBarBackgroundColor(0x3d, 0x3d, 0x3d, 0xff);
Color scrollBarColor(0x80, 0x80, 0x80, 0xff);
Color activeScrollBarColor(0xb4, 0xb4, 0xb4, 0xff);
constexpr int ScrollBarWidth = 17;
constexpr int ScrollBarArrowHeight = 16;
constexpr int ButtonHeight = 20;
constexpr int CheckBoxSize = 12;
Font* g_pFont = 0;

static bool g_state = false;     // mouse button down
static bool g_prevState = false; // previous frame
static int g_mouseX = 0;
static int g_mouseY = 0;
static int g_activeId = -1;

// 0x080f02a0
void setFont(Font* font)
{
    g_activeId = -1;
    g_pFont = font;
}
// 0x080f02c0
void updateInputState(bool mouseDown, int mouseX, int mouseY)
{
    g_prevState = g_state;
    g_state = mouseDown;
    g_mouseX = mouseX;
    g_mouseY = mouseY;
}
static bool inside(int x, int y, int width, int height)
{
    return g_mouseX >= x && g_mouseY >= y && g_mouseX < x + width && g_mouseY < y + height;
}
// 0x080f02f0: press inside grabs the widget, release while grabbed fires it.
bool buttonLogic(int id, int x, int y, int width, int height)
{
    bool over = inside(x, y, width, height);
    if (g_state)
    {
        if (!g_prevState && over && g_activeId < 0)
            g_activeId = id;
        return false;
    }
    if (g_prevState && g_activeId == id)
    {
        g_activeId = -1;
        return over;
    }
    return false;
}

// 0x080f04f0
void drawGroup(int x, int y, int width, int height)
{
    im::fillRoundedRectAA(x, y, width, height, 5.0f, groupColor);
}
// 0x080f0530
void drawVerticalScrollBar(int x, int y, int height, float start, float end, bool active)
{
    float trackHeight = (float)(height - ScrollBarArrowHeight * 2);
    float snapped = (float)lrintf(start * trackHeight) / trackHeight;
    im::fillRect(x, y, ScrollBarWidth, height, scrollBarBackgroundColor);
    float top = (float)(y + ScrollBarArrowHeight);
    float range = (float)(y + height - ScrollBarArrowHeight) - top;
    int y0 = (int)lrintf(snapped * range + top);
    int y1 = (int)lrintf((end - start + snapped) * range + top);
    im::fillRoundedRectAA(x + 5, y0, 7, y1 - y0, 3.0f,
                          active ? activeScrollBarColor : scrollBarColor);
    float cx = (float)x + 8.5f;
    Vec2 up[3] = {Vec2(cx, (float)y + 5.0f), Vec2(cx + 3.0f, (float)y + 11.0f),
                  Vec2(cx - 3.0f, (float)y + 11.0f)};
    im::fillPolygonAA(up, 3, scrollBarColor);
    float bottom = (float)y + (float)height - 5.0f;
    Vec2 down[3] = {Vec2(cx, bottom), Vec2(cx - 3.0f, bottom - 6.0f),
                    Vec2(cx + 3.0f, bottom - 6.0f)};
    im::fillPolygonAA(down, 3, scrollBarColor);
}
// 0x080f0740
void drawSlider(int x, int y, int width, float value, bool active)
{
    im::fillRect(x, y + 4, width, 5, backgroundColor);
    int kx = (int)lrintf((float)width * value) + x;
    Color knob = active ? activeSliderKnobColor : sliderKnobColor;
    Vec2 upper[3] = {Vec2((float)(kx - 4), (float)y), Vec2((float)(kx + 4), (float)y),
                     Vec2((float)kx, (float)(y + 4))};
    im::fillPolygon(upper, 3, knob);
    Vec2 lower[3] = {Vec2((float)kx, (float)(y + 9)), Vec2((float)(kx + 4), (float)(y + 13)),
                     Vec2((float)(kx - 4), (float)(y + 13))};
    im::fillPolygon(lower, 3, knob);
}
static void drawTick(int x, int y)
{
    Vec2 tick[3] = {Vec2(x + 2.5f, y + 5.5f), Vec2(x + 5.0f, y + 8.5f), Vec2(x + 8.5f, y + 2.5f)};
    im::drawPolyLineAA(tick, 3, false, checkBoxTickColor);
}
// 0x080f08f0
void drawCheckBox(int x, int y, bool checked, bool active)
{
    im::fillRect(x, y, CheckBoxSize, CheckBoxSize, backgroundColor);
    im::fillRect(x + 1, y + 1, CheckBoxSize - 2, CheckBoxSize - 2,
                 active ? activeCheckBoxBackgroundColor : checkBoxBackgroundColor);
    if (checked)
        drawTick(x, y);
}
// 0x080f0a10
void drawButton(int x, int y, int width, const char* text, bool active)
{
    im::fillRoundedRectAA(x, y, width, ButtonHeight, 5.0f, backgroundColor);
    im::fillRoundedRectAA(x + 1, y + 1, width - 2, ButtonHeight - 2, 5.0f,
                          active ? activeButtonColor : buttonColor);
    int textWidth = g_pFont->getWidth(text);
    im::drawText(text, (width / 2 + x) - textWidth / 2, y + 4, g_pFont, textColor, INT_MAX);
}
// 0x080f0b00
void drawEditBox(int x, int y, int width, const char* text)
{
    im::fillRoundedRectAA(x, y, width, 20, 5.0f, backgroundColor);
    im::drawText(text, x + 5, y + 4, g_pFont, textColor, INT_MAX);
}

// 0x080f0b80
bool button(int id, int x, int y, int width, const char* text)
{
    bool clicked = buttonLogic(id, x, y, width, 20);
    drawButton(x, y, width, text, id == g_activeId);
    return clicked;
}
// 0x080f0c80: drags the visible range [start, end] with the mouse.
void vertScrollBar(int id, int x, int y, int height, float& start, float& end)
{
    buttonLogic(id, x, y, ScrollBarWidth, height);
    float newEnd = end;
    if (id == g_activeId)
    {
        float center = (float)(g_mouseY - ScrollBarArrowHeight - y) /
                       (float)(height - ScrollBarArrowHeight * 2);
        float half = (end - start) * 0.5f;
        if (center - half < 0.0f)
            center = half;
        newEnd = center + half;
        if (newEnd > 1.0f)
        {
            center = 1.0f - half;
            newEnd = center + half;
        }
        start = center - half;
        end = newEnd;
    }
    drawVerticalScrollBar(x, y, height, start, newEnd, id == g_activeId);
}
// 0x080f0e10
void slider(int id, int x, int y, int width, float& value)
{
    buttonLogic(id, x, y, width, 14);
    float v = value;
    if (id == g_activeId)
    {
        v = (float)(g_mouseX - x) / (float)width;
        if (v < 0.0f)
            v = 0.0f;
        else if (v > 1.0f)
            v = 1.0f;
        value = v;
    }
    drawSlider(x, y, width, v, id == g_activeId);
}
// 0x080f0f60
void editBox(int id, int x, int y, int width, const char* text)
{
    drawEditBox(x, y, width, text);
}
// 0x080f0fe0
void checkBox(int id, int x, int y, bool& checked)
{
    bool over = inside(x, y, 12, 12);
    if (buttonLogic(id, x, y, 12, 12))
        checked = !checked;
    drawCheckBox(x, y, checked, over && g_activeId == id);
}

} // namespace gui
} // namespace engine
