// Reconstructed from Grimrock.bin.x86 Frame.cpp.
#include "Frame.h"
#include "RapidEngine.h"
#include "core/Exception.h"
#include "core/Image.h"
#include "core/Sys.h"
#include <cstring>

using namespace core;

Frame* g_pMainFrame = 0;
Array<Frame*> Frame::sm_frames;
Array<EventHandler*> g_eventHandlers;

// 0x0815b0f0 / 0x0815ae10
void addEventHandler(EventHandler* handler)
{
    g_eventHandlers.push_back(handler);
}
void removeEventHandler(EventHandler* handler)
{
    g_eventHandlers.remove(handler);
}
// 0x0815af00
void shutdownFrame()
{
    g_pMainFrame = 0;
}
// 0x0815b0d0
void resizeFrame(lua_State* L, int width, int height)
{
    g_pMainFrame->getWindow()->setSize(width, height);
}
// 0x0815c6c0
Frame* checkFrame(lua_State* L, int index)
{
    return luax::checkObject<Frame>(L, index);
}

// 0x0812d520: g_keynames
const char* getKeyName(int key)
{
    static const struct
    {
        int key;
        const char* name;
    } names[] = {{8, "backspace"},
                 {9, "tab"},
                 {13, "enter"},
                 {16, "shift"},
                 {17, "control"},
                 {18, "alt"},
                 {19, "pause"},
                 {20, "caps_lock"},
                 {27, "escape"},
                 {32, "space"},
                 {33, "page_up"},
                 {34, "page_down"},
                 {35, "end"},
                 {36, "home"},
                 {37, "left"},
                 {38, "up"},
                 {39, "right"},
                 {40, "down"},
                 {45, "insert"},
                 {46, "delete"},
                 {48, "0"},
                 {49, "1"},
                 {50, "2"},
                 {51, "3"},
                 {52, "4"},
                 {53, "5"},
                 {54, "6"},
                 {55, "7"},
                 {56, "8"},
                 {57, "9"},
                 {65, "A"},
                 {66, "B"},
                 {67, "C"},
                 {68, "D"},
                 {69, "E"},
                 {70, "F"},
                 {71, "G"},
                 {72, "H"},
                 {73, "I"},
                 {74, "J"},
                 {75, "K"},
                 {76, "L"},
                 {77, "M"},
                 {78, "N"},
                 {79, "O"},
                 {80, "P"},
                 {81, "Q"},
                 {82, "R"},
                 {83, "S"},
                 {84, "T"},
                 {85, "U"},
                 {86, "V"},
                 {87, "W"},
                 {88, "X"},
                 {89, "Y"},
                 {90, "Z"},
                 {91, "left_win"},
                 {92, "right_win"},
                 {93, "application"},
                 {96, "numpad_0"},
                 {97, "numpad_1"},
                 {98, "numpad_2"},
                 {99, "numpad_3"},
                 {100, "numpad_4"},
                 {101, "numpad_5"},
                 {102, "numpad_6"},
                 {103, "numpad_7"},
                 {104, "numpad_8"},
                 {105, "numpad_9"},
                 {106, "numpad_multiply"},
                 {107, "numpad_plus"},
                 {109, "numpad_minus"},
                 {110, "numpad_decimal"},
                 {111, "numpad_divide"},
                 {112, "f1"},
                 {113, "f2"},
                 {114, "f3"},
                 {115, "f4"},
                 {116, "f5"},
                 {117, "f6"},
                 {118, "f7"},
                 {119, "f8"},
                 {120, "f9"},
                 {121, "f10"},
                 {122, "f11"},
                 {123, "f12"},
                 {145, "scroll_lock"},
                 {187, "+"},
                 {188, ","},
                 {189, "-"},
                 {190, "."},
                 {0, 0}};
    for (int i = 0; names[i].name; ++i)
        if (names[i].key == key)
            return names[i].name;
    return 0;
}

// 0x0815b630
Frame::Frame() : m_window(0), m_focus(true), m_minimized(false), m_mouseX(0), m_mouseY(0)
{
    memset(m_mouseDown, 0, sizeof(m_mouseDown));
    memset(m_mousePressed, 0, sizeof(m_mousePressed));
#if GRIMROCK_GAME >= 2
    memset(m_mouseReleased, 0, sizeof(m_mouseReleased));
#endif
    memset(m_keyDown, 0, sizeof(m_keyDown));
    memset(m_keyPressed, 0, sizeof(m_keyPressed));
    sm_frames.push_back(this);
}
// 0x0815b520
Frame::~Frame()
{
    if (g_pMainFrame == this)
        g_pMainFrame = 0;
    sm_frames.remove(this);
}
// 0x0815aea0
bool Frame::handle(FocusEvent* e)
{
    m_focus = e->focused;
    return true;
}
// 0x0815aec0: resize reason 0 = minimized
bool Frame::handle(ResizeEvent* e)
{
    m_minimized = e->reason == 0;
    return true;
}
// 0x0815b1c0
void Frame::updateInput()
{
    memset(m_mousePressed, 0, sizeof(m_mousePressed));
#if GRIMROCK_GAME >= 2
    memset(m_mouseReleased, 0, sizeof(m_mouseReleased));
#endif
    memset(m_keyPressed, 0, sizeof(m_keyPressed));
}
// 0x0815ba60
bool Frame::updateFrames()
{
    g_pMainFrame->updateInput();
    for (int i = 0; i < sm_frames.size(); ++i)
        sm_frames[i]->m_events.clear();
    if (g_pMainFrame->m_minimized)
        sysSleep(100);
    return g_pMainFrame->m_window.processMessages();
}
// 0x0815b200: tracks the input state, offers the event to the registered handlers and
// queues it for Frame.pollEvents.
bool Frame::dispatch(Event* e)
{
    switch (e->type)
    {
    case Event_MouseMotion:
    {
        MouseMotionEvent* motion = (MouseMotionEvent*)e;
        m_mouseX = motion->x;
        m_mouseY = motion->y;
        break;
    }
    case Event_MouseButton:
    {
        MouseButtonEvent* button = (MouseButtonEvent*)e;
        m_mouseX = button->x;
        m_mouseY = button->y;
        if (!button->pressed)
        {
#if GRIMROCK_GAME >= 2
            if (m_mouseDown[button->button])
                m_mouseReleased[button->button] = true;
#endif
            m_mouseDown[button->button] = false;
        }
        else
        {
            if (!m_mouseDown[button->button])
                m_mousePressed[button->button] = true;
            m_mouseDown[button->button] = true;
        }
        break;
    }
    case Event_Key:
    {
        KeyEvent* k = (KeyEvent*)e;
        int key = k->key & (NumKeys - 1);
        if (!k->pressed)
        {
            m_keyDown[key] = false;
        }
        else
        {
            if (!m_keyDown[key])
                m_keyPressed[key] = true;
            m_keyDown[key] = true;
        }
        break;
    }
    default:
        break;
    }
    for (int i = 0; i < g_eventHandlers.size(); ++i)
        if (g_eventHandlers[i]->dispatch(e))
            return true;
    QueuedEvent queued;
    memset(&queued, 0, sizeof(queued));
    queued.type = e->type;
    switch (e->type)
    {
    case Event_Key:
    {
        KeyEvent* k = (KeyEvent*)e;
        queued.a = k->key;
        queued.b = k->scancode;
        queued.c = k->ch;
        queued.modifiers = k->modifiers;
        queued.flag = k->pressed;
        break;
    }
    case Event_MouseButton:
    {
        MouseButtonEvent* button = (MouseButtonEvent*)e;
        queued.a = button->x;
        queued.b = button->y;
        queued.c = button->button;
        queued.modifiers = button->modifiers;
        queued.flag = button->pressed;
        break;
    }
    case Event_MouseMotion:
        queued.a = ((MouseMotionEvent*)e)->x;
        queued.b = ((MouseMotionEvent*)e)->y;
        break;
    case Event_MouseWheel:
        memcpy(&queued.a, &((MouseWheelEvent*)e)->delta, sizeof(float));
        break;
    case Event_Menu:
        queued.a = ((MenuEvent*)e)->id;
        break;
    case Event_Focus:
        queued.a = ((FocusEvent*)e)->focused;
        break;
    case Event_Resize:
        queued.a = ((ResizeEvent*)e)->width;
        queued.b = ((ResizeEvent*)e)->height;
        queued.c = ((ResizeEvent*)e)->reason;
        break;
    case Event_Close:
        break;
    default:
        return EventHandler::dispatch(e);
    }
    m_events.push_back(queued);
    return EventHandler::dispatch(e);
}

// ---- Frame bindings --------------------------------------------------------------

static bool optBool(lua_State* L, int index, bool def)
{
    if (lua_isnil(L, index))
        return def;
    luaL_checktype(L, index, LUA_TBOOLEAN);
    return lua_toboolean(L, index) != 0;
}

// 0x0815ccc0: Frame.create{x=, y=, width=, height=, title=, fullscreen=, resizable=, ...}
static int Frame_create(lua_State* L)
{
    constexpr int DefaultWidth = 800;
    constexpr int DefaultHeight = 600;
    int x = 0, y = 0; // the window is centred by Window::open like in the original
    int width = DefaultWidth, height = DefaultHeight;
    const char* title = "";
    bool fullscreen = false, resizable = false, borderless = false;
    Frame* parent = 0;
    if (lua_gettop(L) > 0)
    {
        luaL_checktype(L, 1, LUA_TTABLE);
        lua_getfield(L, 1, "x");
        if (!lua_isnil(L, -1))
            x = luaL_checkinteger(L, -1);
        lua_getfield(L, 1, "y");
        if (!lua_isnil(L, -1))
            y = luaL_checkinteger(L, -1);
        lua_getfield(L, 1, "width");
        if (!lua_isnil(L, -1))
            width = luaL_checkinteger(L, -1);
        lua_getfield(L, 1, "height");
        if (!lua_isnil(L, -1))
            height = luaL_checkinteger(L, -1);
        lua_getfield(L, 1, "title");
        if (!lua_isnil(L, -1))
            title = luaL_checkstring(L, -1);
        lua_getfield(L, 1, "fullscreen");
        fullscreen = optBool(L, -1, false);
        lua_getfield(L, 1, "resizable");
        resizable = optBool(L, -1, false);
        lua_getfield(L, 1, "borderless");
        borderless = optBool(L, -1, false);
        lua_getfield(L, 1, "dialog");
        optBool(L, -1, false);
        lua_getfield(L, 1, "autoClose");
        optBool(L, -1, false);
        lua_getfield(L, 1, "parent");
        if (!lua_isnil(L, -1))
            parent = luax::checkObject<Frame>(L, lua_gettop(L));
    }
    Frame* frame = new Frame;
    int flags = (fullscreen ? Window::Fullscreen : 0) | (resizable ? Window::Resizable : 0) |
                (borderless ? Window::Borderless : 0);
    frame->getWindow()->open(parent ? parent->getWindow() : 0, x, y, width, height, flags, title);
    if (!g_pMainFrame)
        g_pMainFrame = frame;
    if (g_pRapidEngine && g_pRapidEngine->getWindowIcon())
        frame->getWindow()->setIcon(*g_pRapidEngine->getWindowIcon());
    frame->getWindow()->setEventHandler(frame);
    luax::createSharedObject<Frame>(L, frame);
    return 1;
}
static int Frame_setTitle(lua_State* L)
{
    Frame* frame = checkFrame(L, 1);
    String title(luaL_checkstring(L, 2));
    frame->getWindow()->setTitle(title.c_str());
    return 0;
}
static int Frame_setMenuBar(lua_State* L)
{
    Frame* frame = checkFrame(L, 1);
    Menu* menu = 0;
    if (!lua_isnil(L, 2))
        menu = luax::checkObject<Menu>(L, 2);
    frame->getWindow()->setMenuBar(menu);
    return 0;
}
static int Frame_setCursor(lua_State* L)
{
    Frame* frame = checkFrame(L, 1);
    frame->getWindow()->setCursor(luax::checkObject<Cursor>(L, 2));
    return 0;
}
// 0x0815aee0 / 0x004089c0
static int Frame_showCursor(lua_State* L)
{
    Frame* frame = checkFrame(L, 1);
    frame->getWindow()->showCursor(luax::checkBool(L, 2));
    return 0;
}
// 0x0815aef0: no-op on Linux
static int Frame_hide(lua_State* L)
{
    return 0;
}
static int Frame_setMouseMotionMode(lua_State* L)
{
    static luax::Enum modes[] = {{"normal", 0}, {"relative", 1}, {0, 0}};
    Frame* frame = checkFrame(L, 1);
    frame->getWindow()->setMouseMotionMode(luax::checkEnum(L, 2, modes));
    return 0;
}
static int Frame_hasFocus(lua_State* L)
{
    lua_pushboolean(L, checkFrame(L, 1)->hasFocus());
    return 1;
}
static int Frame_getPosition(lua_State* L)
{
    Vec2 pos = checkFrame(L, 1)->getWindow()->getPosition();
    luax::pushVector(L, pos);
    return 1;
}
static int Frame_getSize(lua_State* L)
{
    Window* w = checkFrame(L, 1)->getWindow();
    luax::pushVector(L, Vec2((float)w->getWidth(), (float)w->getHeight()));
    return 1;
}
// 0x0815c0a0: pops the oldest queued event as a table, nothing when the queue is empty.
static int Frame_pollEvents(lua_State* L)
{
    static constexpr const char* typeNames[] = {
        "key", "mouse_button", "mouse_motion", "mouse_wheel", "menu", "focus", "resize", "close"};
    Frame* frame = checkFrame(L, 1);
    Array<Frame::QueuedEvent>& events = frame->getEvents();
    if (events.size() == 0)
        return 0;
    const Frame::QueuedEvent& e = events[0];
    lua_newtable(L);
    lua_pushstring(L, e.type >= 0 && e.type < 8 ? typeNames[e.type] : "???");
    lua_setfield(L, -2, "type");
    switch (e.type)
    {
    case Event_Key:
    {
        const char* name = getKeyName(e.a);
        if (name)
            lua_pushstring(L, name);
        else
            lua_pushfstring(L, "key-%d", e.a);
        lua_setfield(L, -2, "key");
        lua_pushnumber(L, e.a);
        lua_setfield(L, -2, "keyCode");
        if (e.c != 0)
        {
            char ch = (char)e.c;
            lua_pushlstring(L, &ch, 1);
            lua_setfield(L, -2, "char");
        }
        lua_pushnumber(L, e.modifiers);
        lua_setfield(L, -2, "modifiers");
        lua_pushboolean(L, e.flag);
        lua_setfield(L, -2, "down");
        break;
    }
    case Event_MouseButton:
        lua_pushnumber(L, e.a);
        lua_setfield(L, -2, "x");
        lua_pushnumber(L, e.b);
        lua_setfield(L, -2, "y");
        lua_pushnumber(L, e.c);
        lua_setfield(L, -2, "button");
        lua_pushnumber(L, e.modifiers);
        lua_setfield(L, -2, "modifiers");
        lua_pushboolean(L, e.flag);
        lua_setfield(L, -2, "down");
        break;
    case Event_MouseMotion:
        lua_pushnumber(L, e.a);
        lua_setfield(L, -2, "x");
        lua_pushnumber(L, e.b);
        lua_setfield(L, -2, "y");
        break;
    case Event_MouseWheel:
    {
        float delta;
        memcpy(&delta, &e.a, sizeof(float));
        lua_pushnumber(L, delta);
        lua_setfield(L, -2, "delta");
        break;
    }
    case Event_Menu:
        lua_pushnumber(L, e.a);
        lua_setfield(L, -2, "id");
        break;
    case Event_Focus:
        lua_pushboolean(L, e.a != 0);
        lua_setfield(L, -2, "focus");
        break;
    case Event_Resize:
    {
        static constexpr const char* resizeTypes[] = {"minimize", "maximize", "restore"};
        lua_pushnumber(L, e.a);
        lua_setfield(L, -2, "width");
        lua_pushnumber(L, e.b);
        lua_setfield(L, -2, "height");
        lua_pushstring(L, e.c >= 0 && e.c < 3 ? resizeTypes[e.c] : "???");
        lua_setfield(L, -2, "resizeType");
        break;
    }
    default:
        break;
    }
    events.erase(0);
    return 1;
}

#if GRIMROCK_GAME >= 2
// grimrock2.exe 0x00408bf0
static int Frame_pumpMessages(lua_State* L)
{
    Frame::updateFrames();
    return 0;
}
#endif
static const luaL_Reg Frame_methods[] = {{"create", Frame_create},
                                         {"setTitle", Frame_setTitle},
                                         {"setMenuBar", Frame_setMenuBar},
                                         {"setCursor", Frame_setCursor},
                                         {"showCursor", Frame_showCursor},
                                         {"setMouseMotionMode", Frame_setMouseMotionMode},
                                         {"hasFocus", Frame_hasFocus},
                                         {"hide", Frame_hide},
                                         {"getPosition", Frame_getPosition},
                                         {"getSize", Frame_getSize},
                                         {"pollEvents", Frame_pollEvents},
#if GRIMROCK_GAME >= 2
                                         {"pumpMessages", Frame_pumpMessages},
#endif
                                         {0, 0}};

// ---- Menu ------------------------------------------------------------------------

static int Menu_create(lua_State* L)
{
    luax::createSharedObject<Menu>(L, new Menu);
    return 1;
}
static int Menu_addItem(lua_State* L)
{
    Menu* menu = luax::checkObject<Menu>(L, 1);
    int id = luaL_checkinteger(L, 2);
    const char* label = luaL_checkstring(L, 3);
    const char* shortcut = lua_gettop(L) > 3 ? luaL_checkstring(L, 4) : 0;
    menu->addItem(id, label, shortcut);
    return 0;
}
static int Menu_addSubMenu(lua_State* L)
{
    Menu* menu = luax::checkObject<Menu>(L, 1);
    Menu* sub = luax::checkObject<Menu>(L, 2);
    menu->addSubMenu(sub, luaL_checkstring(L, 3));
    return 0;
}
static int Menu_addSeparator(lua_State* L)
{
    luax::checkObject<Menu>(L, 1)->addSeparator();
    return 0;
}
static int Menu_enableItem(lua_State* L)
{
    Menu* menu = luax::checkObject<Menu>(L, 1);
    int id = luaL_checkinteger(L, 2);
    menu->enableItem(id, luax::checkBool(L, 3));
    return 0;
}
static const luaL_Reg Menu_methods[] = {
    {"create", Menu_create},         {"addItem", Menu_addItem},
    {"addSubMenu", Menu_addSubMenu}, {"addSeparator", Menu_addSeparator},
    {"enableItem", Menu_enableItem}, {0, 0}};

// ---- Cursor ----------------------------------------------------------------------

static int Cursor_createSystemCursor(lua_State* L)
{
    try
    {
        static luax::Enum cursors[] = {
            {"arrow", 0},          {"ibeam", 1}, {"wait", 2}, {"hand", 3}, {"split_horizontal", 4},
            {"split_vertical", 5}, {0, 0}};
        int id = luax::checkEnum(L, 1, cursors);
        luax::createSharedObject<Cursor>(L, new Cursor(id));
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x0815bee0 (the original reads the hot spot x twice)
static int Cursor_create(lua_State* L)
{
    try
    {
        const char* filename = luaL_checkstring(L, 1);
        int hotX = luaL_checkinteger(L, 2);
        int hotY = luaL_checkinteger(L, 2);
        Image image(filename);
        luax::createSharedObject<Cursor>(L, new Cursor(image, hotX, hotY));
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
static const luaL_Reg Cursor_methods[] = {
    {"createSystemCursor", Cursor_createSystemCursor}, {"create", Cursor_create}, {0, 0}};

// 0x0815afc0
void frame_mod(lua_State* L)
{
    luax::registerClass(L, "Frame", Frame_methods, 0);
    luax::registerClass(L, "Menu", Menu_methods, 0);
    luax::registerClass(L, "Cursor", Cursor_methods, 0);
}
