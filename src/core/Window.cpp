// Reconstructed from Grimrock.bin.x86 Window.cpp.
#include "core/Window.h"
#include "core/Exception.h"
#include "core/Image.h"
#include "core/Sys.h"
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace core
{

// 0x080cd810
Cursor::Cursor(int systemCursor)
{
    debugPrint("Create system cursor: %d\n", systemCursor);
    m_systemCursor = systemCursor;
    m_pCursor = 0;
}
// 0x080cdab0
Cursor::Cursor(const Image& image, int hotX, int hotY)
{
    SDL_Surface* surface =
        SDL_CreateSurfaceFrom(image.getWidth(), image.getHeight(), SDL_PIXELFORMAT_ARGB8888,
                              (void*)image.getData(), image.getWidth() * 4);
    m_pCursor = SDL_CreateColorCursor(surface, hotX, hotY);
    SDL_DestroySurface(surface);
    m_systemCursor = -1;
}
// 0x080cda90
Cursor::~Cursor()
{
    if (m_pCursor)
        SDL_DestroyCursor(m_pCursor);
}

constexpr int DefaultMenuHeight = 25;

// 0x080d2860
Window::Window(EventHandler* handler)
    : m_pEventHandler(0), m_width(0), m_height(0), m_flags(0), m_mouseMotionMode(0), m_mouseX(0),
      m_mouseY(0), m_savedMouseX(0), m_savedMouseY(0), m_relMouseX(0), m_relMouseY(0), m_pWindow(0),
      m_menuHeight(DefaultMenuHeight)
{
    initializeKeyMap();
    m_pEventHandler = handler;
}
// 0x080ce380
Window::~Window()
{
    if (m_pWindow)
        SDL_DestroyWindow(m_pWindow);
    m_pWindow = 0;
}

// Shared with RenderContextSDL: the GL attributes requested before window creation.
void Window::SetupGLAttributes()
{
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
}

// 0x080cd960
void Window::open(Window* parent, int x, int y, int width, int height, int flags, const char* title)
{
    m_width = width;
    m_height = height;
    m_flags = flags;
    m_title = title;
    // The original creates SDL_WINDOW_FULLSCREEN windows, i.e. it switches the display
    // mode to the configured resolution. On current desktops that leaves the screen black
    // when the mode is not one the panel can show (e.g. a scaled XRandR mode at 85 Hz),
    // so fullscreen keeps the desktop mode and the frame is scaled to it (see
    // RenderContextSDL); the window position is always centred like in the original.
    Uint32 sdlFlags = SDL_WINDOW_OPENGL;
    if (flags & Resizable)
        sdlFlags |= SDL_WINDOW_RESIZABLE;
    if (flags & Borderless)
        sdlFlags |= SDL_WINDOW_BORDERLESS;
    SetupGLAttributes();
    if (flags & Fullscreen)
    {
        // SDL3 fullscreen with a null fullscreen mode is the desktop ("borderless") mode
        m_pWindow = SDL_CreateWindow(title, width, height, sdlFlags | SDL_WINDOW_FULLSCREEN);
        if (m_pWindow)
        {
            SDL_SetWindowFullscreenMode(m_pWindow, 0);
            // wait for the window manager to apply the fullscreen size, otherwise the
            // first frames are presented with the pre-fullscreen window size
            SDL_SyncWindow(m_pWindow);
            SDL_SetWindowMouseGrab(m_pWindow, true);
        }
    }
    else
    {
        m_pWindow = SDL_CreateWindow(title, width, height, sdlFlags);
        if (m_pWindow)
            SDL_SetWindowPosition(m_pWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
    if (!m_pWindow)
        throw Exception("Could not create window: %s", SDL_GetError());
    SDL_StartTextInput(m_pWindow);
}
// Letterboxed rectangle of the render size inside the actual window, in window
// coordinates; equal to the window when the sizes match.
void Window::getPresentationRect(int& x, int& y, int& width, int& height) const
{
    int windowWidth = m_width, windowHeight = m_height;
    if (m_pWindow)
        SDL_GetWindowSize(m_pWindow, &windowWidth, &windowHeight);
    if (windowWidth == m_width && windowHeight == m_height)
    {
        x = 0;
        y = 0;
        width = m_width;
        height = m_height;
        return;
    }
    float scale = getPresentationScale();
    width = (int)lrintf(m_width * scale);
    height = (int)lrintf(m_height * scale);
    x = (windowWidth - width) / 2;
    y = (windowHeight - height) / 2;
}
float Window::getPresentationScale() const
{
    int windowWidth = m_width, windowHeight = m_height;
    if (m_pWindow)
        SDL_GetWindowSize(m_pWindow, &windowWidth, &windowHeight);
    if (m_width <= 0 || m_height <= 0)
        return 1.0f;
    float sx = (float)windowWidth / (float)m_width, sy = (float)windowHeight / (float)m_height;
    return sx < sy ? sx : sy;
}
void Window::windowToRender(int& x, int& y) const
{
    int px, py, pw, ph;
    getPresentationRect(px, py, pw, ph);
    if (pw == m_width && ph == m_height && px == 0 && py == 0)
        return;
    float scale = getPresentationScale();
    x = (int)lrintf((float)(x - px) / scale);
    y = (int)lrintf((float)(y - py) / scale);
}
// 0x080cd900
void Window::close()
{
    if (m_pWindow)
        SDL_DestroyWindow(m_pWindow);
    m_pWindow = 0;
}
// 0x080cd650
void Window::post(Event* e)
{
    if (m_pEventHandler)
        m_pEventHandler->dispatch(e);
}
// KMOD_SHIFT -> 1, KMOD_CTRL -> 2, KMOD_ALT -> 4, KMOD_GUI -> 8
int Window::getModifiers()
{
    int mods = 0;
    SDL_Keymod state = SDL_GetModState();
    if (state & SDL_KMOD_SHIFT)
        mods |= Mod_Shift;
    if (state & SDL_KMOD_CTRL)
        mods |= Mod_Control;
    if (state & SDL_KMOD_ALT)
        mods |= Mod_Alt;
    if (state & SDL_KMOD_GUI)
        mods |= Mod_Gui;
    return mods;
}
// 0x080cdd00
void Window::onKey(const SDL_KeyboardEvent& sdlEvent)
{
    std::map<SDL_Scancode, int>::iterator it = m_keyMap.find(sdlEvent.scancode);
    if (getenv("GRIMROCK_DEBUG_INPUT"))
        debugPrint("key scancode %d keycode %u down %d -> %d\n", (int)sdlEvent.scancode,
                   (unsigned)sdlEvent.key, (int)sdlEvent.down,
                   it == m_keyMap.end() ? -1 : it->second);
    if (it == m_keyMap.end())
        return;
    KeyEvent event;
    event.type = Event_Key;
    event.key = it->second;
    event.scancode = 0;
    event.ch = 0;
    event.modifiers = getModifiers();
    event.pressed = sdlEvent.down;
    post(&event);
}
// 0x080cde20: only single byte characters are delivered.
void Window::onText(const SDL_TextInputEvent& sdlEvent)
{
    if (strlen(sdlEvent.text) < 2)
    {
        KeyEvent event;
        event.type = Event_Key;
        event.key = 0;
        event.scancode = 0;
        event.ch = sdlEvent.text[0];
        event.modifiers = 0;
        event.pressed = true;
        post(&event);
    }
}
// 0x080cdc10
void Window::onMouseButton(const SDL_MouseButtonEvent& sdlEvent)
{
    MouseButtonEvent event;
    event.type = Event_MouseButton;
    event.x = (int)lrintf(sdlEvent.x);
    event.y = (int)lrintf(sdlEvent.y);
    windowToRender(event.x, event.y);
    event.button = sdlEvent.button - 1;
    event.modifiers = getModifiers();
    event.pressed = sdlEvent.down;
    post(&event);
}
// 0x080cddc0
void Window::onMouseMove(const SDL_MouseMotionEvent& sdlEvent)
{
    if (m_mouseMotionMode == MouseMotion_Relative)
    {
        // relative deltas keep the render-space sensitivity when the frame is scaled
        float scale = getPresentationScale();
        m_relMouseX += (int)lrintf(sdlEvent.xrel / scale);
        m_relMouseY += (int)lrintf(sdlEvent.yrel / scale);
        return;
    }
    int x = (int)lrintf(sdlEvent.x), y = (int)lrintf(sdlEvent.y);
    windowToRender(x, y);
    m_mouseX = x;
    m_mouseY = y;
    MouseMotionEvent event;
    event.type = Event_MouseMotion;
    event.x = x;
    event.y = y;
    post(&event);
}
// 0x080cdcc0
void Window::onMouseWheel(const SDL_MouseWheelEvent& sdlEvent)
{
    MouseWheelEvent event;
    event.type = Event_MouseWheel;
    event.delta = (float)sdlEvent.y;
    post(&event);
}
// 0x080cdb40
void Window::onMenu(int id)
{
    MenuEvent event;
    event.type = Event_Menu;
    event.id = id;
    post(&event);
}
// 0x080cdc90
void Window::onFocus(bool focused)
{
    FocusEvent event;
    event.type = Event_Focus;
    event.focused = focused;
    post(&event);
}
// 0x080cdba0
void Window::onResize(int width, int height)
{
    ResizeEvent event;
    event.type = Event_Resize;
    event.width = width;
    event.height = height;
    event.reason = 2;
    post(&event);
}
// 0x080cd6b0 (only repositioned the FLTK menu window)
void Window::onMove(int x, int y) {}
// 0x080cdb70
void Window::onClose()
{
    CloseEvent event;
    event.type = Event_Close;
    post(&event);
}

// 0x080ce820
bool Window::processMessages()
{
    SDL_Event sdlEvent;
    while (SDL_PollEvent(&sdlEvent))
    {
        switch (sdlEvent.type)
        {
        case SDL_EVENT_QUIT:
            return false;
        case SDL_EVENT_WINDOW_MOVED:
            onMove(sdlEvent.window.data1, sdlEvent.window.data2);
            break;
        case SDL_EVENT_WINDOW_RESIZED:
            onResize(sdlEvent.window.data1, sdlEvent.window.data2);
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            onFocus(true);
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            onFocus(false);
            break;
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            onClose();
            break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            onKey(sdlEvent.key);
            break;
        case SDL_EVENT_TEXT_INPUT:
            onText(sdlEvent.text);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            onMouseMove(sdlEvent.motion);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            onMouseButton(sdlEvent.button);
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            onMouseWheel(sdlEvent.wheel);
            break;
        default:
            if (Menu::sdlUserEvent && sdlEvent.type == Menu::sdlUserEvent)
                onMenu(sdlEvent.user.code);
            break;
        }
    }
    if (m_mouseMotionMode == MouseMotion_Relative)
    {
        MouseMotionEvent event;
        event.type = Event_MouseMotion;
        event.x = m_relMouseX;
        event.y = m_relMouseY;
        post(&event);
        m_relMouseX = 0;
        m_relMouseY = 0;
    }
    return true;
}

// 0x080cd710
void Window::setMouseMotionMode(int mode)
{
    if (m_mouseMotionMode == mode)
        return;
    m_mouseMotionMode = mode;
    if (mode == MouseMotion_Relative)
    {
        m_savedMouseX = m_mouseX;
        m_savedMouseY = m_mouseY;
        SDL_HideCursor();
        SDL_SetWindowMouseGrab(m_pWindow, true);
        SDL_SetWindowRelativeMouseMode(m_pWindow, true);
        return;
    }
    {
        int px, py, pw, ph;
        getPresentationRect(px, py, pw, ph);
        float scale = getPresentationScale();
        SDL_WarpMouseInWindow(m_pWindow, px + m_savedMouseX * scale, py + m_savedMouseY * scale);
    }
    SDL_ShowCursor();
    SDL_SetWindowRelativeMouseMode(m_pWindow, false);
    if (!(m_flags & Fullscreen))
        SDL_SetWindowMouseGrab(m_pWindow, false);
}
// 0x080cd7c0
Vec2 Window::getPosition() const
{
    int x = 0, y = 0;
    SDL_GetWindowPosition(m_pWindow, &x, &y);
    return Vec2((float)x, (float)y);
}
// 0x080cd850
void Window::setSize(int width, int height)
{
    m_width = width;
    m_height = height;
    SDL_SetWindowSize(m_pWindow, width, height);
}
// 0x080cded0
void Window::setTitle(const char* title)
{
    m_title = title;
    SDL_SetWindowTitle(m_pWindow, title);
}
// 0x080cd880
void Window::setIcon(const Image& image)
{
    SDL_Surface* surface =
        SDL_CreateSurfaceFrom(image.getWidth(), image.getHeight(), SDL_PIXELFORMAT_ARGB8888,
                              (void*)image.getData(), image.getWidth() * 4);
    SDL_SetWindowIcon(m_pWindow, surface);
    SDL_DestroySurface(surface);
}
// 0x080ce340
void Window::setCursor(Cursor* cursor)
{
    m_cursor.reset(cursor);
    if (cursor)
        SDL_SetCursor(cursor->getSDLCursor());
}
// 0x080ce640
void Window::setMenuBar(Menu* menu)
{
    m_menu.reset(menu);
    if (menu)
        debugPrint("Loading Menu Bar: %p\n", (void*)menu);
}

// 0x080cec80: SDL scancodes to Windows virtual key codes. The table is reproduced
// verbatim, including the original's F6..F12 being shifted by one.
void Window::initializeKeyMap()
{
    static constexpr int table[][2] = {
        {39, 0x30},  {30, 0x31},  {31, 0x32},  {32, 0x33},  {33, 0x34},  {34, 0x35},  {35, 0x36},
        {36, 0x37},  {37, 0x38},  {38, 0x39},  {4, 0x41},   {5, 0x42},   {6, 0x43},   {7, 0x44},
        {8, 0x45},   {9, 0x46},   {10, 0x47},  {11, 0x48},  {12, 0x49},  {13, 0x4a},  {14, 0x4b},
        {15, 0x4c},  {16, 0x4d},  {17, 0x4e},  {18, 0x4f},  {19, 0x50},  {20, 0x51},  {21, 0x52},
        {22, 0x53},  {23, 0x54},  {24, 0x55},  {25, 0x56},  {26, 0x57},  {27, 0x58},  {28, 0x59},
        {29, 0x5a},  {42, 0x8},   {43, 0x9},   {40, 0xd},   {57, 0x14},  {41, 0x1b},  {44, 0x20},
        {75, 0x21},  {78, 0x22},  {77, 0x23},  {74, 0x24},  {80, 0x25},  {82, 0x26},  {79, 0x27},
        {81, 0x28},  {73, 0x2d},  {76, 0x2e},  {98, 0x60},  {89, 0x61},  {90, 0x62},  {91, 0x63},
        {92, 0x64},  {93, 0x65},  {94, 0x66},  {95, 0x67},  {96, 0x68},  {97, 0x69},  {85, 0x6a},
        {87, 0x6b},  {86, 0x6d},  {220, 0x6e}, {84, 0x6f},  {58, 0x70},  {59, 0x71},  {60, 0x72},
        {61, 0x73},  {62, 0x74},  {63, 0x76},  {64, 0x77},  {65, 0x78},  {66, 0x79},  {67, 0x7a},
        {68, 0x7b},  {69, 0x7c},  {83, 0x90},  {71, 0x91},  {227, 0x5b}, {231, 0x5c}, {225, 0x10},
        {229, 0x10}, {224, 0x11}, {228, 0x11}, {226, 0x12}, {230, 0x12}, {51, 0xba},  {46, 0xbb},
        {54, 0xbc},  {45, 0xbd},  {55, 0xbe},  {56, 0xbf},  {53, 0xc0},  {47, 0xdb},  {49, 0xdc},
        {48, 0xdd},  {52, 0xde},  {100, 0xe2}};
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); ++i)
        m_keyMap[(SDL_Scancode)table[i][0]] = table[i][1];
}

} // namespace core
