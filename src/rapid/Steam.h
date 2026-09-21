// The Steam module (Steam.cpp, 0x081307d0-0x08134350): Lua bindings for the Steamworks
// API (achievements, stats, cloud storage, workshop) and the callback context that
// queues Steam events for Steam.pollEvents. Built against the Steamworks SDK headers in
// third_party/steamworks; the original links Steamworks 1.23a (SteamClient012 etc.).
#pragma once
#include "luax.h"

void steam_mod(lua_State* L);
void shutdownSteam();
