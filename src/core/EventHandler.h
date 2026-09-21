// Window events, reconstructed from EventHandler.cpp (0x080c6de0-0x080c6f50) and the
// event structures built in Window.cpp.
#pragma once

namespace core
{

enum EventType
{
    Event_Key = 0,
    Event_MouseButton = 1,
    Event_MouseMotion = 2,
    Event_MouseWheel = 3,
    Event_Menu = 4,
    Event_Focus = 5,
    Event_Resize = 6,
    Event_Close = 7
};

enum KeyModifier
{
    Mod_Shift = 1,
    Mod_Control = 2,
    Mod_Alt = 4,
    Mod_Gui = 8
};

struct Event
{
    int type;
};
// Key presses carry the virtual key code; text input carries the character in ch.
struct KeyEvent : Event
{
    int key;
    int scancode;
    int ch;
    int modifiers;
    bool pressed;
};
struct MouseButtonEvent : Event
{
    int x, y;
    int button; // 0 = left, 1 = middle, 2 = right
    int modifiers;
    bool pressed;
};
struct MouseMotionEvent : Event
{
    int x, y; // absolute, or relative deltas in relative mouse mode
};
struct MouseWheelEvent : Event
{
    float delta;
};
struct MenuEvent : Event
{
    int id;
};
struct FocusEvent : Event
{
    bool focused;
};
struct ResizeEvent : Event
{
    int width, height;
    int reason;
};
struct CloseEvent : Event
{
};

class EventHandler
{
  public:
    virtual ~EventHandler() {}
    // 0x080c6de0
    virtual bool dispatch(Event* e);
    virtual bool handle(KeyEvent* e)
    {
        return false;
    }
    virtual bool handle(MouseButtonEvent* e)
    {
        return false;
    }
    virtual bool handle(MouseMotionEvent* e)
    {
        return false;
    }
    virtual bool handle(MouseWheelEvent* e)
    {
        return false;
    }
    virtual bool handle(MenuEvent* e)
    {
        return false;
    }
    virtual bool handle(FocusEvent* e)
    {
        return false;
    }
    virtual bool handle(ResizeEvent* e)
    {
        return false;
    }
    virtual bool handle(CloseEvent* e)
    {
        return false;
    }
};

} // namespace core
