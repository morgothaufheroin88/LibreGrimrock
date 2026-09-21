# LibreGrimrock

A source reconstruction of the Linux build of *Legend of Grimrock* (Almost Human,
build 2013-05-15, `Grimrock.bin.x86`), written in C++ from the decompiled i386
binary and compiled as a native **64-bit** executable.

The shipped Linux binary is 32-bit only. It still runs on most distributions
through the Steam Linux Runtime, but it relies on the old 32-bit `stat` ABI
(`EOVERFLOW` on file systems with 64-bit inodes), a 32-bit address space and a
2013 driver environment, and it cannot be maintained. The primary goal of this
project is therefore a faithful **64-bit port**: the same compilation units,
class layouts, Lua bindings and quirks as the original, with changes limited to
what a 64-bit build and modern libraries require. The second goal, once the
first game is complete, is the same treatment for *Legend of Grimrock 2*, which
shares most of this engine.

The game data (`grimrock.dat`, textures, scripts) and the original executable
are not part of this repository: you need to own the game (Steam, GOG or the
Humble build). This project only replaces the executable.

## State

The whole game runs from the original `grimrock.dat`: main menu, intro,
character creation, dungeon exploration, saving and loading, the settings, the
dungeon editor and the Steam integration (achievements, cloud saves, workshop)
have been exercised with the reconstructed executable.

- `src/core` — the core library: strings, containers, math, streams, the GRA2
  and ZIP file systems, images, SDL2 window and input, dialogs, threads, debug
  drawing.
- `src/engine` — the engine: light pre-pass GL renderer with shadows, SSAO,
  FXAA and particles, the notebook (low quality) renderer, immediate mode
  drawing and fonts, mesh/model/animation formats, asset pipeline, the null
  physics engine of the Linux build, OpenAL audio with Ogg streaming.
- `src/rapid` — the Lua layer: the `luax` object model, the `sys`, core,
  frame, engine (42 classes, 537 bindings) and Steam modules, the `RapidEngine`
  main loop and `main`.
- `tests` — unit tests of the core library.
- `tools` — the deployment and dependency scripts, the archive tool and the
  reverse engineering helpers described below.

Not reconstructed: the FBX/LWO/material library import paths (never reached by
the shipped Lua scripts) and the PhysX, FMOD, XAudio2 and Direct3D back ends
that the Linux binary does not contain.

## Building

Requirements: a C++17 compiler, CMake ≥ 3.20, SDL3, OpenAL, libvorbisfile,
zlib, minizip, FreeType, GLEW, OpenGL, `pkg-config`, Git and Python 3 (tools
only). Image loading uses the vendored `stb_image` headers. On a Debian/Ubuntu
system:

```sh
sudo apt install build-essential cmake pkg-config git libsdl3-dev \
    libopenal-dev libvorbis-dev zlib1g-dev libminizip-dev libfreetype-dev \
    libglew-dev
```

Two third-party pieces are not redistributed here and are fetched by a script:

- **LuaJIT 2.0.0-rc3**, the revision linked into the original binary. Its
  bytecode loader reads the precompiled scripts inside `grimrock.dat` (LuaJIT
  2.1 cannot).
- **Steamworks SDK** (optional). Valve's SDK licence does not allow
  redistribution; download `steamworks_sdk.zip` from
  <https://partner.steamgames.com/downloads/steamworks_sdk.zip> (a free
  Steamworks account is needed) and pass it to the script. Without the SDK the
  game is built with a Steam stub and runs in its non-Steam mode.

```sh
tools/fetch_deps.sh [~/Downloads/steamworks_sdk.zip]
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j$(nproc)
ctest --test-dir build-release
```

## Running

The executable expects `grimrock.dat` and `grimrock.png` next to itself and
changes to its own directory at start (like the original); symlinks are fine:

```sh
cd build-release
ln -s "$HOME/.local/share/Steam/steamapps/common/Legend of Grimrock/grimrock.dat" .
ln -s "$HOME/.local/share/Steam/steamapps/common/Legend of Grimrock/grimrock.png" .
./grimrock
```

Configuration (`grimrock.cfg`), saves and `error.log` live in
`~/.local/share/Almost Human/Legend of Grimrock/`, the same place the original
uses, so existing saves are picked up.

With a Steam client running, `Steam.init` connects to it when a file
`steam_appid.txt` containing `207170` lies next to the executable; without the
file `SteamAPI_RestartAppIfNecessary` relaunches the game through Steam (the
behaviour of the original).

`build-release/grimrock_archive grimrock.dat list | exists NAME | read NAME OUT |
extract-all DIR | verify` inspects the game archive.

## Installing into the Steam copy

`tools/deploy_steam.sh [game dir]` builds `build-release` and installs the
result over the Steam installation (default
`~/.local/share/Steam/steamapps/common/Legend of Grimrock`):

- the stripped executable replaces `Grimrock.bin.x86` (the original is kept as
  `Grimrock.bin.x86.orig`);
- `libsteam_api.so` (64-bit, from the SDK) is placed next to it;
- `lib64/` receives the host libraries that the Steam Linux Runtime container
  does not provide (`libminizip`, `libGLEW`, `libSDL3`); the executable's
  `RUNPATH` is `$ORIGIN/lib64:$ORIGIN`. OpenAL, FreeType, vorbisfile and zlib
  come from the runtime.

After that the game starts from the Steam client as usual, with the overlay,
achievements, cloud saves and workshop working. Steam's *Verify integrity of
game files* restores the original binary; run the script again afterwards.

## Compatibility changes

The reconstruction follows the original compilation units, class layouts and
Lua registration order; it is not a rewrite. Behaviour that differs from the
i386 binary is limited to:

- **64-bit**: modern `stat`/`fstat` and 64-bit sizes in the file systems, a
  memory mapped archive instead of 32-bit offsets, pointer-sized Lua proxies,
  refcount map and `uint64` userdata.
- **Libraries**: the SDL2 window/input layer of the original is on SDL3;
  FreeImage, FLTK and binreloc are replaced by `stb_image`, `kdialog` and
  `SDL_GetBasePath`; the linked-in LuaBitOp is replaced by
  LuaJIT's own `bit` library (same API); Steamworks 1.23a by the current SDK
  (only `ISteamUserStats::RequestCurrentStats` no longer exists — the binding
  reports success, the stats arrive automatically).
- **Fullscreen** does not switch the display mode. The original opens an
  exclusive `SDL_WINDOW_FULLSCREEN` window at the configured resolution, which
  on today's desktops means an XRandR mode change and a black screen when the
  panel cannot show that mode. Fullscreen now uses the desktop mode; the frame
  is rendered into an off-screen buffer of the configured size and scaled,
  letterboxed, onto the window, with mouse coordinates mapped back.
- **Frame rate**: the original spins in a busy loop until `1/maxFrameRate`
  (120 in the shipped config) has passed. The cap is now the refresh rate of
  the display and the wait sleeps instead of burning a core; with vertical sync
  on, the swap does the pacing anyway.
- A handful of real bugs of the original are fixed where they corrupt memory
  (`String::erase` over-read, a use-after-free of a mesh source copy) or leak
  (Steam tag arrays); each is commented at the site.

## Reverse engineering workflow

The source was reconstructed from Ghidra pseudocode of `Grimrock.bin.x86`
(project "LoG"); that pseudocode is derived from the copyrighted binary and is
kept out of the repository, as are the decompiled game scripts. The tools that
compare the reconstruction with it expect the evidence under `reverse/`:

- `reverse/symtab.txt` — `readelf -sW Grimrock.bin.x86` (the binary is not
  stripped, all 2,433 game/engine symbols have names);
- `reverse/native/<address>.c` — per-function pseudocode exported with
  `tools/export_native.py` from a running Ghidra instance with the
  [GhidraMCP](https://github.com/LaurieWired/GhidraMCP) HTTP bridge;
- `reverse/cu-map.json` — the compilation unit of each function (from the
  `_GLOBAL__I_*` anchors and the symbol order).

Every reconstructed function carries a `// 0x<address>` comment above its
signature. With the evidence in place, `tools/sidebyside.py CU` prints the
pseudocode next to our function (`tools/sbs.sh` strips refcount noise),
`tools/luareg.py <module address>` and `tools/bindings.py` recover the Lua
registration tables, `tools/rdstr.py` reads strings and enum tables, and
`tools/constcheck.py`, `intcheck.py`, `strcheck.py`, `throwcheck.py`,
`stubcheck.py`, `coverage.py` report literals, error paths and functions that
differ between the original and the reconstruction. `tools/catalog_lua.py` and
`tools/check_lua.py` index and validate decompiled Lua drafts against the
bytecode in the archive (using [ljd](https://github.com/Aussiemon/ljd)).

Debugging aids (all off by default): `GRIMROCK_CAPTURE_DIR=<dir>` saves the next
frame to `<dir>/capture.png` when `<dir>/take` exists
(`tools/devtest/cap.sh`, independent of the compositor);
`GRIMROCK_DEBUG_LIGHTS=1` dumps the scene lights, `GRIMROCK_DEBUG_AUDIO=1`
logs stream files, end-of-stream events and starving streams;
`GRIMROCK_DEBUG_STALLS=<ms>` starts a watchdog thread that prints the main
thread's backtrace when a frame takes longer than the given time (plus the
Lua heap, RSS and live shared object count every 10 s — for the Steam copy
put `GRIMROCK_DEBUG_STALLS=500 %command%` into the launch options and read
`~/.local/share/Steam/logs/console-linux.txt`); `tools/devtest/lua.sh` pastes Lua into the in-game
console (`console = true` in `grimrock.cfg`, opened with the backslash key);
`-DGRIMROCK_PATTERN_INIT_FILES=a.cpp;b.cpp` compiles the listed sources with
pattern-initialised locals.

## Code style

`clang-format` with the repository `.clang-format` (Allman braces, 4-space
indent, 100 columns). Constants are named `constexpr` values; class, function
and member names are the original symbols from the binary's symbol table and
are the link to `reverse/symtab.txt`, so they are not renamed.

## License

The reconstructed sources, tools and tests are released under the GNU General
Public License, version 3 or later (see `LICENSE`). *Legend of Grimrock*, its
data files and the original executable are the property of Almost Human Ltd.
and are not included; this project is not affiliated with or endorsed by
Almost Human. The Steamworks SDK is subject to Valve's SDK licence and must be
obtained from Valve; LuaJIT is MIT licensed.
