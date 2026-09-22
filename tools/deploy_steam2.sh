#!/bin/sh
# Builds the Release executable of the second game and installs it into the Steam copy of
# Legend of Grimrock 2:
#   grimrock2             <- stripped build-release/grimrock, the launcher of both games;
#                            started under this name it runs the second game
#   libgrimrock2.so       <- the engine of the second game, which the launcher loads
#   grimrock2.png         <- the window icon, taken out of grimrock2.exe (tools/log2/icon.py)
#   libsteam_api.so       <- Steamworks SDK linux64 library
#   lib64/                <- host libraries the Steam Linux Runtime container does not ship
# The second game has no Linux depot, so the Windows install is only the source of the
# data: the Windows executable is left alone and ours is added next to it. Run it from the
# game directory, or point Steam at it with a launch option.
# Usage: tools/deploy_steam2.sh [game dir]
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
GAME=${1:-"$HOME/.local/share/Steam/steamapps/common/Legend of Grimrock 2"}
[ -f "$GAME/grimrock2.dat" ] || { echo "not a Grimrock 2 install: $GAME" >&2; exit 1; }

cmake -S "$ROOT" -B "$ROOT/build-release" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$ROOT/build-release" --target grimrock -j"$(nproc)"

strip -o "$GAME/grimrock2" "$ROOT/build-release/grimrock"
chmod +x "$GAME/grimrock2"
strip -o "$GAME/libgrimrock2.so" "$ROOT/build-release/libgrimrock2.so"
[ -f "$GAME/grimrock2.png" ] || python3 "$ROOT/tools/log2/icon.py" "$GAME/grimrock2.exe" "$GAME/grimrock2.png"
[ -f "$GAME/steam_appid.txt" ] || echo 251730 > "$GAME/steam_appid.txt"
if [ -f "$ROOT/third_party/steamworks/linux64/libsteam_api.so" ]; then
    cp "$ROOT/third_party/steamworks/linux64/libsteam_api.so" "$GAME/libsteam_api.so"
fi

mkdir -p "$GAME/lib64"
ldd "$ROOT/build-release/grimrock" "$ROOT/build-release/libgrimrock2.so" | awk '/libminizip|libGLEW|libSDL3/ { print $1, $3 }' |
while read -r name path; do
    cp -L "$path" "$GAME/lib64/$name"
done
echo "installed into $GAME"
