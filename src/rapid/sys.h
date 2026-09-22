// The sys module and the global functions of the Lua environment (sys.cpp,
// 0x0812d520-0x0812fb30).
#pragma once
#include "core/ArchiveFileSystem.h"
#include "core/ByteArrayStream.h"
#include "core/FileStream.h"
#include "core/FileSystem.h"
#include "core/Image.h"
#include "luax.h"

// Class table names of the core module's types (Core.cpp).
LUAX_CLASS(core::FileSystem, "FileSystem")
LUAX_CLASS(core::ArchiveFileSystem, "ArchiveFileSystem")
LUAX_CLASS(core::InputStream, "InputStream")
LUAX_CLASS(core::OutputStream, "OutputStream")
LUAX_CLASS(core::FileInputStream, "FileInputStream")
LUAX_CLASS(core::FileOutputStream, "FileOutputStream")
LUAX_CLASS(core::ByteArrayInputStream, "ByteArrayInputStream")
LUAX_CLASS(core::ByteArrayOutputStream, "ByteArrayOutputStream")
LUAX_CLASS(core::FileDate, "FileDate")
LUAX_CLASS(core::Image, "Image")

void sys_mod(lua_State* L);
void core_mod(lua_State* L);
#if GRIMROCK_GAME >= 2
// src2/rapid/SaveGame.cpp (0x00431640)
void savegame_mod(lua_State* L);
#endif
// Resets the delta time measurement (0x0812d570).
void resetDeltaTime();
// Calls the function set by sys.displayFunc with the traceback handler at errfunc.
void callDisplayFunc(lua_State* L, int errfunc);
