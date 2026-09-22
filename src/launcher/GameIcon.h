// The icon of a game for the launcher: the PNG the game ships with (grimrock.png), or, for
// the second game, the icon in the resources of its Windows executable (the one the Windows
// build hands to LoadIcon, group 0x65), read the same way tools/log2/icon.py does.
#pragma once
#include "GameLibrary.h"
#include <SDL3/SDL.h>
#include <string>

namespace launcher
{

// null when the install has neither
SDL_Texture* loadGameIcon(SDL_Renderer* renderer, const GameInfo& game,
                          const std::string& directory);

} // namespace launcher
