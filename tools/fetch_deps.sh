#!/bin/sh
# Fetches and builds the third-party pieces that are not redistributed with this repository:
#   third_party/luajit     LuaJIT 2.0.0-rc3 (the revision linked into the original binary; its
#                          bytecode loader reads the precompiled scripts in grimrock.dat)
#   third_party/steamworks the Steamworks SDK (headers + linux64/libsteam_api.so), which
#                          Valve does not allow to be redistributed: download it from
#                          https://partner.steamgames.com/downloads/steamworks_sdk.zip
#                          (a free Steamworks account is required) and pass the zip.
# Usage: tools/fetch_deps.sh [steamworks_sdk.zip]
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$ROOT/third_party"

LUAJIT_REV=87d74a8f3d8f5a53fc7ad1fd45adcc06db4bcde8   # RELEASE LuaJIT-2.0.0-rc3
if [ ! -f "$ROOT/third_party/luajit/src/libluajit.a" ]; then
    if [ ! -d "$ROOT/third_party/luajit" ]; then
        git clone --branch v2.0 https://github.com/LuaJIT/LuaJIT.git "$ROOT/third_party/luajit"
    fi
    git -C "$ROOT/third_party/luajit" checkout --quiet "$LUAJIT_REV"
    make -C "$ROOT/third_party/luajit" -j"$(nproc)" BUILDMODE=static
fi

if [ -n "$1" ]; then
    tmp=$(mktemp -d)
    unzip -q "$1" -d "$tmp"
    sdk=$(find "$tmp" -maxdepth 2 -type d -name sdk | head -1)
    [ -n "$sdk" ] || { echo "no sdk/ directory in $1" >&2; exit 1; }
    mkdir -p "$ROOT/third_party/steamworks/linux64"
    cp -r "$sdk/public/steam" "$ROOT/third_party/steamworks/"
    cp "$sdk/redistributable_bin/linux64/libsteam_api.so" "$ROOT/third_party/steamworks/linux64/"
    cp "$sdk/Readme.txt" "$ROOT/third_party/steamworks/" 2>/dev/null || true
    rm -rf "$tmp"
fi
if [ ! -f "$ROOT/third_party/steamworks/steam/steam_api.h" ]; then
    echo "note: Steamworks SDK not installed in third_party/steamworks (pass the SDK zip);" >&2
    echo "      the Steam module will be built without it (Steam.init reports failure)." >&2
fi
echo "dependencies ready"
