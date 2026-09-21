// Native dialogs, from Dialog.cpp (0x080d3730). The original used FLTK's fl_file_chooser /
// fl_dir_chooser; this build shells out to kdialog (or zenity) instead.
#pragma once
#include "core/String.h"

namespace core
{

class Window;

enum FileDialogMode
{
    FileDialog_Open = 0,
    FileDialog_Save = 1
};

String fileDialog(Window* parent, int mode, const char* title, const char* description,
                  const char* pattern, const char* defaultName);
String browseFolderDialog(Window* parent, const char* title);

} // namespace core
