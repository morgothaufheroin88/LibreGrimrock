// Reconstructed from Grimrock.bin.x86 Menu.cpp.
#include "core/Menu.h"
#include <SDL2/SDL.h>

namespace core
{

unsigned int Menu::sdlUserEvent = 0;

// 0x080cbd40
void Menu::setupSDL()
{
    if (sdlUserEvent == 0)
        sdlUserEvent = SDL_RegisterEvents(1);
}
// 0x080cbd70
Menu::Menu()
{
    setupSDL();
}
// 0x080cbca0
Menu::~Menu() {}
// 0x080cc010
void Menu::addItem(int id, const char* label, const char* shortcut)
{
    Item& item = m_items.push_back();
    item.id = id;
    item.label = label;
    item.shortcut = shortcut ? shortcut : "";
}
// 0x080cbef0
void Menu::addSubMenu(Menu* menu, const char* label)
{
    Item& item = m_items.push_back();
    item.label = label;
    item.subMenu.reset(menu);
}
// 0x080cbbe0
void Menu::addSeparator()
{
    if (m_items.size() > 0)
        m_items.back().separatorAfter = true;
}
// 0x080cbe60
void Menu::addEnd() {}
// 0x080cbc30
void Menu::enableItem(int id, bool enabled)
{
    for (int i = 0; i < m_items.size(); ++i)
    {
        if (m_items[i].id == id)
        {
            m_items[i].enabled = enabled;
            return;
        }
        if (m_items[i].subMenu)
            m_items[i].subMenu->enableItem(id, enabled);
    }
}
// 0x080cbcf0 cb_MenuClick: post the item id as an SDL user event.
bool Menu::trigger(int id)
{
    for (int i = 0; i < m_items.size(); ++i)
    {
        if (m_items[i].id == id)
        {
            if (!m_items[i].enabled)
                return false;
            SDL_Event e;
            SDL_memset(&e, 0, sizeof(e));
            e.type = sdlUserEvent;
            e.user.code = id;
            return SDL_PushEvent(&e) == 1;
        }
        if (m_items[i].subMenu && m_items[i].subMenu->trigger(id))
            return true;
    }
    return false;
}

} // namespace core
