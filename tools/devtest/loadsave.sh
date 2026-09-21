#!/bin/sh
# usage: loadsave.sh BUILDDIR LOG [SLOT] — starts the game (config: fullscreen at the desktop
# resolution 2560x1440 or windowed 1280x720; coordinates are scaled by SCALE, default 2 for
# a 1280x720 frame on a 2560x1440 screen), loads save SLOT (1..3) and leaves the game running.
B=$1; LOG=$2; SLOT=${3:-2}; SCALE=${SCALE:-2}
export GRIMROCK_CAPTURE_DIR=${GRIMROCK_CAPTURE_DIR:-/tmp/grimrock-cap}; mkdir -p "$GRIMROCK_CAPTURE_DIR"
pkill -x grimrock; sleep 1
cd "$B" && (stdbuf -oL ./grimrock > "$LOG" 2>&1 &)
for i in $(seq 1 60); do sleep 1; W=$(xdotool search --name "Legend of Grimrock" | head -1); [ -n "$W" ] && break; done
sleep 10
X=$(xdotool getwindowgeometry --shell "$W" | sed -n 's/^X=//p'); Y=$(xdotool getwindowgeometry --shell "$W" | sed -n 's/^Y=//p')
click() { xdotool mousemove $((X + $1 * SCALE)) $((Y + $2 * SCALE)); sleep 0.6; xdotool mousedown 1; sleep 0.15; xdotool mouseup 1; }
click 640 292; sleep 3                       # Load game
xdotool mousemove $((X + 640 * SCALE)) $((Y + (172 + 56 * SLOT) * SCALE)); sleep 0.6; xdotool click 1; sleep 0.3; xdotool click 1
sleep 12
