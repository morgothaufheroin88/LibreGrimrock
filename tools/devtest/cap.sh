#!/bin/sh
# usage: cap.sh OUT.png — asks the running game (started with GRIMROCK_CAPTURE_DIR=$CAP) for a
# back-buffer capture; independent of the compositor (X root captures of GL windows can be black).
CAP=${GRIMROCK_CAPTURE_DIR:-/tmp/grimrock-cap}; mkdir -p "$CAP"
touch "$CAP/take"; for i in $(seq 1 40); do sleep 0.1; [ -f "$CAP/take" ] || break; done; sleep 1.5
mv "$CAP/capture.png" "$1" 2>/dev/null && magick "$1" -format "%[fx:mean]\n" info:
