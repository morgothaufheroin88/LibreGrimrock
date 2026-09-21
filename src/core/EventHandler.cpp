// Reconstructed from Grimrock.bin.x86 EventHandler.cpp.
#include "core/EventHandler.h"

namespace core
{

// 0x080c6de0
bool EventHandler::dispatch(Event* e)
{
    switch (e->type)
    {
    case Event_Key:
        return handle((KeyEvent*)e);
    case Event_MouseButton:
        return handle((MouseButtonEvent*)e);
    case Event_MouseMotion:
        return handle((MouseMotionEvent*)e);
    case Event_MouseWheel:
        return handle((MouseWheelEvent*)e);
    case Event_Menu:
        return handle((MenuEvent*)e);
    case Event_Focus:
        return handle((FocusEvent*)e);
    case Event_Resize:
        return handle((ResizeEvent*)e);
    case Event_Close:
        return handle((CloseEvent*)e);
    default:
        return false;
    }
}

} // namespace core
