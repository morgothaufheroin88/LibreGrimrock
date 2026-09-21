#!/bin/sh
# usage: lua.sh 'lua code' — pastes the code into the open in-game console (config console=true,
# opened with the backslash key) and runs it; print() output goes to the game's stdout.
printf '%s' "$1" | xclip -selection clipboard
xdotool key ctrl+v; sleep 0.4; xdotool key Return; sleep 0.6
