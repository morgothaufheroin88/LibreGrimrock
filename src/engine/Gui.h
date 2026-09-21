// Immediate mode debug GUI, reconstructed from Gui.cpp (0x080f02a0-0x080f0fe0).
#pragma once
#include "core/Color.h"

namespace engine
{

class Font;

namespace gui
{

extern core::Color backgroundColor;
extern core::Color textColor;
extern core::Color groupColor;
extern core::Color buttonColor;
extern core::Color activeButtonColor;
extern core::Color checkBoxBackgroundColor;
extern core::Color checkBoxTickColor;
extern core::Color sliderKnobColor;
extern core::Color scrollBarBackgroundColor;
extern core::Color scrollBarColor;
extern Font* g_pFont;

void setFont(Font* font);
void updateInputState(bool mouseDown, int mouseX, int mouseY);
// Returns true when the button is released over the rectangle.
bool buttonLogic(int id, int x, int y, int width, int height);

void drawGroup(int x, int y, int width, int height);
void drawVerticalScrollBar(int x, int y, int height, float start, float end, bool active);
void drawSlider(int x, int y, int width, float value, bool active);
void drawCheckBox(int x, int y, bool checked, bool active);
void drawButton(int x, int y, int width, const char* text, bool active);
void drawEditBox(int x, int y, int width, const char* text);

bool button(int id, int x, int y, int width, const char* text);
void vertScrollBar(int id, int x, int y, int height, float& start, float& end);
void slider(int id, int x, int y, int width, float& value);
void editBox(int id, int x, int y, int width, const char* text);
void checkBox(int id, int x, int y, bool& checked);

} // namespace gui
} // namespace engine
