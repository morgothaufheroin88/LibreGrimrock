// Menu bar description, reconstructed from Menu.cpp (0x080cbbe0-0x080cc010). The original
// built an FLTK Fl_Menu_Item array and posted an SDL user event on clicks; without FLTK
// the items are kept as data and activated through Menu::trigger().
#pragma once
#include "core/Array.h"
#include "core/SharedPtr.h"
#include "core/String.h"

namespace core
{

class Menu
{
  public:
    struct Item
    {
        int id;
        String label;
        String shortcut;
        bool enabled;
        bool separatorAfter;
        SharedPtr<Menu> subMenu;
        Item() : id(0), enabled(true), separatorAfter(false) {}
    };

    Menu();
    virtual ~Menu();
    void addItem(int id, const char* label, const char* shortcut);
    void addSubMenu(Menu* menu, const char* label);
    void addSeparator();
    void addEnd();
    void enableItem(int id, bool enabled);
    bool trigger(int id);
    const Array<Item>& items() const
    {
        return m_items;
    }

    static unsigned int sdlUserEvent;
    static void setupSDL();

  private:
    Array<Item> m_items;
};

} // namespace core
