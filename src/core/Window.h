// SDL3 window, cursor and event translation, reconstructed from Window.cpp (SDL2 in the original)
// (0x080cd630-0x080d2860). The original also owned an FLTK menu bar window; menus are
// kept as data here (see Menu.h).
#pragma once
#include "core/EventHandler.h"
#include "core/Menu.h"
#include "core/SharedPtr.h"
#include "core/String.h"
#include "core/Vector.h"
#include <SDL3/SDL.h>
#include <map>

namespace core
{

class Image;

class Cursor
{
  public:
    // 0x080cd810: SDL system cursor id
    explicit Cursor(int systemCursor);
    // 0x080cdab0: BGRA image with hot spot
    Cursor(const Image& image, int hotX, int hotY);
    ~Cursor();
    SDL_Cursor* getSDLCursor() const
    {
        return m_pCursor;
    }
    int getSystemCursor() const
    {
        return m_systemCursor;
    }

  private:
    SDL_Cursor* m_pCursor;
    int m_systemCursor;
};

class Window
{
  public:
    // Flag values as passed by the original Frame.create (4 is the borderless window).
    enum Flags
    {
        Fullscreen = 1,
        Resizable = 2,
        Borderless = 4
    };
    enum MouseMotionMode
    {
        MouseMotion_Absolute = 0,
        MouseMotion_Relative = 1
    };

    explicit Window(EventHandler* handler);
    virtual ~Window();

    void open(Window* parent, int x, int y, int width, int height, int flags, const char* title);
    void close();
    bool processMessages();
    void post(Event* e);
    void setEventHandler(EventHandler* handler)
    {
        m_pEventHandler = handler;
    }
    void setMouseMotionMode(int mode);
    int getMouseMotionMode() const
    {
        return m_mouseMotionMode;
    }
    Vec2 getPosition() const;
    void setSize(int width, int height);
    void setTitle(const char* title);
    void setIcon(const Image& image);
    void setCursor(Cursor* cursor);
    void setMenuBar(Menu* menu);
    int getWidth() const
    {
        return m_width;
    }
    int getHeight() const
    {
        return m_height;
    }
    int getFlags() const
    {
        return m_flags;
    }
    const String& getTitle() const
    {
        return m_title;
    }
    SDL_Window* getSDLWindow() const
    {
        return m_pWindow;
    }
    static void SetupGLAttributes();
    // The requested (render) size may differ from the actual window size: fullscreen uses
    // the desktop resolution and the frame is scaled into it (letterboxed). These map
    // window coordinates to render coordinates.
    void getPresentationRect(int& x, int& y, int& width, int& height) const;
    void windowToRender(int& x, int& y) const;
    float getPresentationScale() const;

  private:
    void initializeKeyMap();
    void onKey(const SDL_KeyboardEvent& e);
    void onText(const SDL_TextInputEvent& e);
    void onMouseButton(const SDL_MouseButtonEvent& e);
    void onMouseMove(const SDL_MouseMotionEvent& e);
    void onMouseWheel(const SDL_MouseWheelEvent& e);
    void onMenu(int id);
    void onFocus(bool focused);
    void onResize(int width, int height);
    void onMove(int x, int y);
    void onClose();
    static int getModifiers();

    EventHandler* m_pEventHandler;
    int m_width;
    int m_height;
    int m_flags;
    String m_title;
    int m_mouseMotionMode;
    SharedPtr<Menu> m_menu;
    SharedPtr<Cursor> m_cursor;
    int m_mouseX, m_mouseY;
    int m_savedMouseX, m_savedMouseY;
    int m_relMouseX, m_relMouseY;
    SDL_Window* m_pWindow;
    [[maybe_unused]] int m_menuHeight; // Windows menu bar, kept for the original layout
    std::map<SDL_Scancode, int> m_keyMap;
};

} // namespace core
