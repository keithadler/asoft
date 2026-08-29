#!/bin/sh
# The keyboard, from the command line rather than the windowed front end.
#
# The console build behaves differently when its input is a terminal: the
# keyboard can be polled without waiting, which is what a game needs. A pipe
# cannot show that, so this runs it under a pty.
#
# Two things are checked. A program spinning on PEEK(-16384) has to see a key
# pressed while it runs. And SNAKE.BAS -- a real game, polling the same way --
# has to notice Q and quit, which is a different ending from running into a
# wall, so seeing QUIT proves the key arrived rather than the snake dying.
set -e
cd "$(dirname "$0")/.."

strobe=build/cli-keys.out
printf '10 FOR T = 1 TO 4000000\n20 K = PEEK(-16384)\n30 IF K > 127 THEN 60\n40 NEXT T\n50 PRINT "NO KEY SEEN": END\n60 POKE -16368,0: PRINT "GOT ";CHR$(K-128)\n70 END\nRUN\nX\nWAIT:15\n' \
    | python3 tools/ptyrun.py --settle 0.4 -- ./build/asoft > "$strobe" 2>&1

if ! grep -aq "GOT X" "$strobe"; then
    echo "run_cli_keys: FAILED - a running program never saw the key"
    grep -aE "NO KEY|GOT" "$strobe" | head -3
    exit 1
fi

game=build/cli-game.out
printf 'Q\nWAIT:6\n' \
    | python3 tools/ptyrun.py --settle 0.3 -- ./build/asoft -r web/bundle/SNAKE.BAS \
    > "$game" 2>&1

if ! LC_ALL=C sed 's/\x1b\[[0-9;]*m//g' "$game" | grep -aq "QUIT - SCORE"; then
    echo "run_cli_keys: FAILED - the game did not see Q"
    LC_ALL=C sed 's/\x1b\[[0-9;]*m//g' "$game" | grep -aE "QUIT|GAME OVER" | head -3
    exit 1
fi

echo "run_cli_keys: a running game reads the keyboard from the command line"
