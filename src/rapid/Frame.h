// Frame: the application window as seen from Lua, reconstructed from Frame.cpp
// (0x0815aea0-0x0815d620). Layout follows the original (0x4a0 bytes).
#pragma once
#include "core/Array.h"
#include "core/EventHandler.h"
#include "core/Window.h"
#include "luax.h"

class Frame : public core::EventHandler
{
  public:
    static constexpr int NumKeys = 512;
    static constexpr int NumMouseButtons = 3;
    // Events queued for Lua (0x1c bytes each in the original).
    struct QueuedEvent
    {
        int type;
        int a, b, c, d;
        int modifiers;
        bool flag;
    };

    Frame();
    ~Frame();
    bool dispatch(core::Event* e);
    bool handle(core::FocusEvent* e);
    bool handle(core::ResizeEvent* e);
    // Clears the per frame pressed flags.
    void updateInput();
    // 0x0815ba60: resets input state of all frames and pumps the window messages.
    static bool updateFrames();

    core::Window* getWindow()
    {
        return &m_window;
    }
    core::Array<QueuedEvent>& getEvents()
    {
        return m_events;
    }
    bool hasFocus() const
    {
        return m_focus;
    }
    bool isMinimized() const
    {
        return m_minimized;
    }
    int getMouseX() const
    {
        return m_mouseX;
    }
    int getMouseY() const
    {
        return m_mouseY;
    }
    bool isMouseDown(int button) const
    {
        return m_mouseDown[button];
    }
    bool isMousePressed(int button) const
    {
        return m_mousePressed[button];
    }
    bool isKeyDown(int key) const
    {
        return m_keyDown[key & (NumKeys - 1)];
    }
    bool isKeyPressed(int key) const
    {
        return m_keyPressed[key & (NumKeys - 1)];
    }

    static core::Array<Frame*> sm_frames;

  private:
    core::Window m_window;
    core::Array<QueuedEvent> m_events;
    bool m_focus;
    bool m_minimized;
    int m_mouseX;
    int m_mouseY;
    bool m_mouseDown[NumMouseButtons];
    bool m_mousePressed[NumMouseButtons];
    bool m_keyDown[NumKeys];
    bool m_keyPressed[NumKeys];
};

LUAX_CLASS(Frame, "Frame")
LUAX_CLASS(core::Menu, "Menu")
LUAX_CLASS(core::Cursor, "Cursor")

extern Frame* g_pMainFrame;
extern core::Array<core::EventHandler*> g_eventHandlers;

void addEventHandler(core::EventHandler* handler);
void removeEventHandler(core::EventHandler* handler);
void resizeFrame(lua_State* L, int width, int height);
Frame* checkFrame(lua_State* L, int index);
void shutdownFrame();
// Virtual key code -> name ("backspace", "A", "f1"...), 0 when unnamed (0x0812d520).
const char* getKeyName(int key);
void frame_mod(lua_State* L);
