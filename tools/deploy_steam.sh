#!/bin/sh
# Builds the Release executable and installs it over the Steam copy of the game:
#   Grimrock.bin.x86      <- stripped build-release/grimrock, the launcher of both games
#                            (original kept as .orig); it finds grimrock.dat next to it
#   libgrimrock1.so       <- the engine of the first game, which the launcher loads
#   libsteam_api.so       <- Steamworks SDK linux64 library
#   lib64/                <- host libraries that the Steam Linux Runtime (scout) container
#                            does not ship (minizip, GLEW, SDL3); everything else (OpenAL,
#                            FreeType, vorbisfile, zlib, libstdc++) comes from the runtime.
# Usage: tools/deploy_steam.sh [game dir]
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
GAME=${1:-"$HOME/.local/share/Steam/steamapps/common/Legend of Grimrock"}
[ -f "$GAME/grimrock.dat" ] || { echo "not a Grimrock install: $GAME" >&2; exit 1; }

cmake -S "$ROOT" -B "$ROOT/build-release" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$ROOT/build-release" --target grimrock -j"$(nproc)"

if [ -f "$GAME/Grimrock.bin.x86" ] && [ ! -f "$GAME/Grimrock.bin.x86.orig" ] &&
   ! file "$GAME/Grimrock.bin.x86" | grep -q "64-bit"; then
    cp -p "$GAME/Grimrock.bin.x86" "$GAME/Grimrock.bin.x86.orig"
fi
strip -o "$GAME/Grimrock.bin.x86" "$ROOT/build-release/grimrock"
chmod +x "$GAME/Grimrock.bin.x86"
strip -o "$GAME/libgrimrock1.so" "$ROOT/build-release/libgrimrock1.so"
if [ -f "$ROOT/third_party/steamworks/linux64/libsteam_api.so" ]; then
    cp "$ROOT/third_party/steamworks/linux64/libsteam_api.so" "$GAME/libsteam_api.so"
fi

mkdir -p "$GAME/lib64"
ldd "$ROOT/build-release/grimrock" "$ROOT/build-release/libgrimrock1.so" | awk '/libminizip|libGLEW|libSDL3/ { print $1, $3 }' |
while read -r name path; do
    cp -L "$path" "$GAME/lib64/$name"
done
echo "installed into $GAME"
