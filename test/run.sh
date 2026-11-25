#!/bin/sh
# Host tests for q2-connect4 (x86-64 Linux, clang, python3 with Pillow): the rules and the AI, then
# the real game and platform layer on fbshim.so's fake /dev/fb0, driven through Q2 Pod's input
# socket (test/drive.py). Each scenario must end on TERM with status 0, the panel back on Q2 Pod's
# page and the socket gone. Frames land in test/out/*.raw and .png.
set -e
cd "$(dirname "$0")/.."
mkdir -p test/out
./test/hostbuild.sh
./build/host/logic_test
for sc in menus friend ai pause quit; do
    python3 -I test/drive.py build/host test/out $sc > test/out/$sc.log 2>&1 || { cat test/out/$sc.log; exit 1; }
    grep -q "^exit status: 0" test/out/$sc.log || { echo "FAIL: $sc: exit status"; cat test/out/$sc.log; exit 1; }
    grep -q "^panel: yoffset 0 " test/out/$sc.log || { echo "FAIL: $sc: panel not handed back"; exit 1; }
    grep -q "^socket left: False" test/out/$sc.log || { echo "FAIL: $sc: socket left behind"; exit 1; }
    echo "$sc: ok"
done
res() { grep "^result: $2:" test/out/$1.log | sed 's/.*: //'; }
fail() { echo "FAIL: $*"; exit 1; }
[ "$(res menus sel_after_wheel)" = 2 ] || fail "menus: the wheel didn't move the highlight down"
[ "$(res menus level_changed)" = 1 ] || fail "menus: Centre didn't change the AI level"
[ "$(res menus started)" = 1 ] && [ "$(res menus mode)" = 0 ] || fail "menus: Play vs AI didn't start"
[ "$(res friend mode)" = 1 ] || fail "friend: not a two-player game"
[ "$(res friend winner)" = 1 ] && [ "$(res friend p1)" = 1 ] && [ "$(res friend over)" = 3 ] || fail "friend: red's four didn't win"
[ "$(res friend next_starter)" = 2 ] && [ "$(res friend next_turn)" = 2 ] || fail "friend: yellow doesn't start the next game"
[ "$(res ai level)" = 0 ] || fail "ai: not Easy"
[ "$(res ai over)" = 3 ] || fail "ai: the game didn't finish"
[ "$(res ai back_to_title)" = 0 ] || fail "ai: Return didn't go back to the menu"
[ "$(res pause held_return)" = 2 ] || fail "pause: a held Return didn't open the pause once"
[ "$(res pause moves_before)" = 2 ] || fail "pause: the AI didn't answer"
[ "$(res pause after_undo_state)" = 1 ] && [ "$(res pause after_undo_moves)" = 0 ] && [ "$(res pause after_undo_turn)" = 1 ] ||
    fail "pause: Take Back didn't take back your move and the AI's"
[ "$(res pause menu)" = 0 ] || fail "pause: Main Menu"
[ "$(res quit exited_alone)" = 1 ] || fail "quit: Quit didn't end the game"
grep -h "AI (" test/out/*.log | head -3
for f in test/out/*.raw; do python3 -I test/view.py "$f" "${f%.raw}.png"; done
echo "all ok: menus, two players by wheel and touch, a game against the AI, pause, Take Back, Quit"
