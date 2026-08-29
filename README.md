# Applesoft BASIC for DOS

An Applesoft interpreter that keeps the ROM's bugs on purpose, with a console
front end and a Turbo Vision one, running in the browser under js-dos.

```
]LOAD TESTS.BAS
]RUN
FLOATS:
.3 .333333333 1024 1.41421356
1E+09 1E-03 .01 999999999
```

`.1 + .2` prints `.3` because Applesoft carried nine significant digits, not
because anything was rounded for show. `HTAB 10 : PRINT POS(0)` answers 11,
which is wrong by two and is what the ROM does. Trapping errors in a loop
kills the program with `?OUT OF MEMORY` after exactly sixty of them, because
`ONERR` leaks a stack frame every time. All of that is reproduced deliberately
and can be switched off individually.

## Layout

```
src/          the interpreter, portable C89
  mbf.c         5-byte MBF floats: pack, unpack, and Applesoft's PRINT format
  token.c       the ROM keyword table, tokenizer and LIST expander
  a2mem.c       the 64K memory image: program, variables, arrays, strings, GC
  gfx.c         lo-res and hi-res plotting into real page memory
  screen.c      the 40-column screen: wrapping, comma zones, HTAB, POS
  interp.c      expressions, statements, control stack, ONERR
  panes.c       what the Machine pane says
  bugs.c        which ROM misfeatures are switched on
  main_stdio.c  the console front end
  main_tv.cpp   the Turbo Vision front end (Borland only; see below)
tests/        unit tests, plus transcript replays against the reference build
tools/
  capture/      the js-dos rig that recorded the reference build's behaviour
  layout.c      draws the whole 80x43 Turbo Vision screen as text
web/          the js-dos host page and bundle
reference/    the original ASOFT.EXE, kept for comparison
```

## Building

The host build needs nothing but a C compiler:

```sh
make          # build/asoft and build/layout
make check    # unit tests and transcript replays
./build/asoft web/bundle/TESTS.BAS
```

`build/layout` draws the Turbo Vision screen as text, from live state, which is
how the pane contents get checked without a Borland toolchain:

```sh
./build/layout -c web/bundle/TESTS.BAS
```

For the 16-bit DOS console build, see `build-dos.sh` (Open Watcom, no install
needed). For the Turbo Vision build, see `makefile.bc` — that one needs Borland
C++ 3.1 under DOS, because Turbo Vision exists for no other compiler.

## Where the behaviour came from

The archive this started from contained a working `ASOFT.EXE` and no `src/`.
Rather than guess at the ROM from memory, the binary was run under js-dos and
its behaviour recorded: `tools/capture/` pipes a script through the interpreter
inside DOSBox, reads the transcript back out of the emulator's filesystem, and
`tests/run_capture.sh` replays the same script through this build and diffs.

That is what settled the things nobody should guess at — that `LIST` prints
keywords as `" KEYWORD "` and drops the spaces you typed, that the `]` prompt
does not count towards the 40-column wrap, that `DIM B(10)` costs 62 bytes,
that the ONERR leak is exactly sixty deep. `tools/capture/README.md` lists the
measurements.

Replaying `TESTS.BAS` currently matches the reference on 61 of 62 lines, and
the wide regression matches on all 33.

## Known differences from the reference build

Deliberate, except the first:

| | this build | reference |
|---|---|---|
| `FRE(0)` after `TESTS.BAS` | 35492 | 35491 |
| `PRINT 1E-5` | `1E-05` | `-4` |
| `LEFT$("ABC",0)` | `""` | `?ILLEGAL QUANTITY` |
| `LOAD`, `SAVE`, calling `DEF FN` | work | `?SYNTAX ERROR` |
| `PLOT`, `HPLOT` | draw into page memory | parsed and ignored |

The `FRE(0)` byte is the one unexplained difference. Every allocation rule was
measured and matched — program storage is byte-exact line by line, scalars cost
7, arrays 7+5n, literals cost nothing, `READ` copies — so what is left is a
single byte of collector residue inside a binary whose source is gone.

`PRINT 1E-5` printing `-4` is the reference stopping after the `E` and reading
`-5` as subtraction. The ROM accepts signed exponents, so this build does too.

## The bugs, and switching them off

`bug_enabled[]` in `src/bugs.c`, the **Bugs** menu in the Turbo Vision build,
or `./build/asoft -n` to disable all four.

- **ONERR leak.** Every trapped error pushes a frame that is never popped.
  240 bytes of control stack, 4 bytes a time, so the sixty-first error is
  fatal. `CALL -3288` pops one, which is what `ONERRFIX.BAS` demonstrates.
- **HTAB off-by-two.** `HTAB n` leaves the cursor at column n+1, not n-1.
  `POS` then reports that position honestly.
- **MBF rounding.** Results are rounded to a 32-bit mantissa after every
  operation. Switch it off and `.1 + .2` stops printing as `.3`.
- **Greedy tokenizer.** Keywords match anywhere, so `TOTAL = 5` tokenizes as
  `TO TAL = 5`. Switching it off costs you `FORI=1TO10`, which then needs its
  spaces.

## Status

The interpreter, the console front end and the web bundle work and are tested.
`src/main_tv.cpp` has **not been compiled** — there is no 16-bit Borland
toolchain here and Turbo Vision does not build under Open Watcom, so it is
written to the API but unverified. Its pane contents and layout are checked
separately through `panes.c` and `tools/layout.c`.

Shape-table `DRAW`/`XDRAW`, sound, and `PR#`/`IN#` are parsed and ignored.

## Where it differs from the reference

Two places, both measured rather than assumed:

- `FRE(0)` reads one byte higher than the reference: 35492 against 35491, one
  byte of collector residue.
- `ATN` differs by one in the ninth significant digit on some arguments --
  `ATN(1)` gives .785398163 against the reference's .785398164. Every other
  function matches exactly. The reference is not using the host's `atan`, nor
  the Applesoft ROM's polynomial: neither reproduces its values, and the
  differences run in both directions, which is the signature of some third
  approximation. Twelve sampled arguments were not enough to identify it, so
  this is left as a known difference rather than guessed at.

The tokenizer bug behind `ATN` was real and is fixed: `AT` is a prefix of
`ATN` and the table listed `AT` first, so `ATN(1)` came apart into `AT`
followed by the variable `N`. Keyword matching now takes the longest match
rather than the first. That does not soften the deliberate greedy bug, because
`TOTAL` is not itself a keyword and still becomes `TO` + `TAL`.

## Graphics

`GR` and `HGR` write into the Apple's page memory, laid out the way the
hardware wanted it rather than the way anything wants to draw it: hi-res rows
are interleaved in three groups of eight, with eight unused bytes at the end
of each group. `PEEK` sees exactly what the machine would have seen.

Two front ends display those pages. The DOS build uses VGA mode 13h, where
320x200 holds hi-res (280x192) with a border and lo-res (40x48) at exactly 8x4
per cell; plotted pixels go straight to video memory as they happen, and a
`POKE` or a screen clear triggers a full repaint. The native build draws the
same pages with terminal half-blocks.

### Colour is a property of position

`HCOLOR` does not choose a colour so much as a bit pattern. Green lights only
odd columns and violet only even ones, so a single green dot at an even `x`
draws nothing at all. The pixel clock ran at the colour subcarrier frequency,
so where a dot sat decided what came out of the display:

- a lit pixel reads white if either neighbour is lit
- a lone dot fills a whole colour cycle, and so is two screen pixels wide

Which is why `HPLOT 0,0 TO 0,191` in white comes out **violet**: a one-pixel
vertical line has no horizontal neighbour, so it takes its column's colour. On
an odd column the same line is green. That is the hardware, not a bug, and it
is why hi-res art is full of two-pixel-wide strokes.

The widening happens in the rasterizer rather than in the plot, so page memory
stays honest: `PEEK` still sees the single bit that was set.

### Shape tables

`DRAW`, `XDRAW`, `ROT=` and `SCALE=` work. A shape is a string of moves packed
three to a byte -- two that can plot and a third that only moves -- and a zero
byte ends it. The packing rules are awkward, because a section cannot always
hold what you want without the byte reading as the terminator; `tools/mkshape.py`
encodes vectors into a table and emits the `DATA` statements.

`ROT=` and `SCALE=` live in zero page at $F9 and $E7, so `POKE 249,16` and
`ROT=16` are the same thing. `SCALE=0` means 256, not "no scale". Every plot
that lands on an already-lit pixel bumps the collision counter at $EA, so a
program can tell that two shapes overlap without reading the screen back.

One deliberate divergence: the ROM only turns cleanly at the quadrants, and in
between walks a distorted version of the shape that depends on the scale.
`ROT=` here rounds to the nearest quadrant instead -- right on the multiples of
16 that programs actually use, an approximation elsewhere.

### Programs

Ten ship in `web/bundle`, and the IDE's Samples menu loads any of them in one
pick. From a shell:

    ./build/asoft -r web/bundle/MANDEL.BAS     # banded by escape time
    ./build/asoft -r web/bundle/SIERP.BAS      # Sierpinski, by the chaos game
    ./build/asoft -r web/bundle/SPIRO.BAS      # hypotrochoids
    ./build/asoft -r web/bundle/MOIRE.BAS      # interference
    ./build/asoft -r web/bundle/HGRDEMO.BAS    # the colour rules themselves
    ./build/asoft -r web/bundle/HGRSHAP.BAS    # scale, rotation, XDRAW, collisions
    ./build/asoft -r web/bundle/LORES.BAS      # all sixteen lo-res colours
    ./build/asoft -r web/bundle/SNAKE.BAS      # I J K M to steer, Q to quit

Mandelbrot in particular is worth looking at: it is banded by escape time into
the four hi-res colours, which is exactly the palette Apple II fractal art of
the period had to work with.

### The keyboard, for games

`GET` stops and waits, which is no use to anything that has to keep moving.
Programs of the period polled the keyboard soft switches instead, and those
work here: `PEEK(-16384)` returns the last key with bit 7 set while it is
unread, and `POKE -16368,0` acknowledges it. SNAKE.BAS is built on that, so it
keeps going while you decide where to turn.

`build/hgrdump program.bas out.bmp [scale]` renders a program's graphics page
to a file, which is how the colour rules are checked without squinting at a
terminal.

While hi-res is up the DOS build prints nothing at all, because that is what
the Apple did: the prompt went to the text page, which was not the page on
screen. Type `TEXT` blind and press return to get back. The terminal build
does not do this -- it has only one screen, and hiding the prompt there would
just look broken.

