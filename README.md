# Connect Four for the Shanling Q2

Connect Four as a [Q2 Pod](https://github.com/DiamondBond/q2-pod) card game: it plays from
**Library → Games** on the Q2, with no Rockbox. Play against the AI (Easy, Medium or Hard) or pass
the Q2 to a friend. The music keeps playing while you play.

| | |
|---|---|
| Wheel | move the disc over the columns (the highlight in menus) |
| Centre | drop the disc (pick, in menus) |
| Touch | finger on a column to aim, lift to drop; tap menu items |
| Return | pause: Resume, Take Back, Restart, Main Menu |
| Quit | on the main menu: leave the game |
| Hold Return | leave the game (Q2 Pod's own) |

**AI levels** (`src/logic.c`): negamax with alpha-beta, scoring positions by the open lines of four.
Easy looks one move ahead and only sometimes takes a win or blocks one; Medium looks four ahead and
always wins or blocks when it can, but about one move in three plays any column that doesn't lose
at once; Hard looks six ahead and always plays its best. The slowest Hard move looks at about
5,600 positions.

The player's guide (install, controls, levels) is [card/README.txt](card/README.txt), copied into
the card folder.

## Building

```sh
./build.sh       # build/q2-connect4 and dist/.connect4 (copy .connect4 to the card's root)
test/run.sh      # host tests: the rules and AI, then the real main() on a fake Q2 panel
```

The build needs `clang`, `ld.lld` and `python3` with Pillow. `tools/font.py` bakes the type into the
binary in ITC Avant Garde Gothic Bold: by default TeX Gyre Adventor Bold, its free clone; set
`FONT_BOLD` to a licensed ITC Avant Garde Bold file to use that instead. `tools/logo.py` bakes the
title logo from `art/logo.png`.
It's built as Challenger Deep is, with no MIPS sysroot, to the ABI of Q2 Pod's own helpers: MIPS32r2
little-endian, o32, FR=1, NaN2008, glibc 2.28, interpreter `/lib/ld-linux-mipsn8.so.1`.
`sysroot/stubs/gen.py` makes a stub `libc.so.6` to link against; a new libc call goes there too.

The card folder holds `q2app` (`name=Connect Four`, `game=1`, no `audio=1`, so Q2 Pod leaves the
music playing), the icons from `tools/icon.py`, and the binary. The game writes `connect4.sav`
(AI level and the last mode played) and `connect4.log` beside itself.

## Licences

Code: MIT ([LICENSE](LICENSE)). TeX Gyre Adventor: GUST Font License.

"Connect Four" and the Connect 4 logo (`art/logo.png`) are Hasbro's. This is an unofficial fan game, not affiliated with or
endorsed by Hasbro.
