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

## Running other people's programs

`tools/corpus.py <directory>` runs a pile of third-party Applesoft programs
and reports what breaks. The corpus is not in this repository -- it is other
people's code under their own licences -- so clone some and point the tool at
it. It is not a pass/fail suite: many of those programs are interactive, or
expect a disk, and stopping early is often correct. What it is good for is
finding places where this interpreter refuses something real Applesoft
accepted, because a syntax error in a program that ran on the hardware is a
bug here, not there.

Over 116 programs from eight repositories it found four, all since fixed:

- **`?` was not PRINT.** Applesoft stores `?` as the PRINT token, so a program
  typed with `?` lists back as though it never had one.
- **`NOT` was only usable at the head of an expression**, so
  `POKE 49236 + NOT SC,0` -- real code -- was a syntax error. It is a unary
  operator wherever a term can start, taking its operand at relational
  precedence.
- **`ATN` came apart into `AT` + `N`.**
- **`A TO 3` came apart into `AT` + `O3`.** Keyword matching ignores spaces,
  which is how `PR INT` becomes PRINT -- and is exactly why `A TO` matched AT
  across the gap. The ROM resolves it by looking at what follows a matched
  AT: an N means the word was ATN, an O means it was really TO.

A fifth thing it found was not a bug but a missing feature: `NEXT I,J`. Real
Applesoft has always taken a list of variables there and programs written for
real machines use it, so that is implemented -- see the note below, because it
is the one place this deliberately parts company with the reference.

Six programs still fail, and none of them is a bug here: one relies on `DIM A`
without a subscript, which the reference rejects too; one is defeated by the
greedy tokenizer, which is a deliberate ROM bug and which `-n` gets past; one
is Microsoft BASIC (`DEFINT A-Z`); one uses string `DEF FN`, which Applesoft
has never had; and two are wrapped listings rather than source, their DATA
statements continued across lines with no line number.

### Answering the prompts

`--feed` answers INPUT and GET instead of closing stdin, which is the
difference between a program stopping at its first question and actually
running. It roughly doubles what gets exercised: `do_input`, the keyboard
strobe, `ONERR`, and the long tail of a program's own logic.

Fed input, the same 116 programs give 50 clean runs, 22 that raise an error
inside a program, and 44 still going when the clock runs out -- mostly games
waiting for a better answer than a canned one. Every one of the 22 is
accounted for:

- **13 syntax errors**, all of them the greedy tokenizer doing its job or code
  that was never Applesoft. `INWORD` becomes `INW OR D`, `renew` becomes
  `re NEW`, `elevation` becomes `elev AT ion`, `score` becomes `SC OR E`. So
  does `IF NOT A THEN`, which is worth knowing: `A THEN` matches AT across the
  space and leaves `HEN`. The reference does exactly the same thing, byte for
  byte -- a variable called `A` in front of `THEN` really did break on the
  hardware, and only `TO` and `ATN` get rescued.
- **5 bad subscripts**, from canned answers steering past the ten elements an
  undimensioned array gets. Auto-dimensioning itself works.
- **3 out of memory**, two of them genuinely enormous -- `DIM L(128,24,2)` and
  `DIM P(64,2,70,3)` do not fit in a 64K image, and did not fit in a real one.
- **1 out of data**, a program reading past its DATA.

Nothing in that list is an interpreter bug. Re-entering a FOR was checked
separately, since a loop exhausting memory would have been one: 200 re-entries
of the same loop variable reuse the frame rather than stacking up.

### The bugs earn their keep

Turning the ROM bugs off makes *more* programs fail, not fewer -- 28 against
6. The greedy tokenizer is why: matching keywords anywhere is what lets
`FORI=1TO3` be read as `FOR I = 1 TO 3`, and period programs are written
without spaces because the machine did not need them. Switching it off buys
you `TOTAL` as a variable and costs you every program that ran the words
together. The default is the authentic one, and it is also the one that runs
more real code.

## Where it differs from the reference

Three places, all measured rather than assumed:

Two of them are deliberate, and both follow the hardware rather than the
reference. Neither can be checked against it, so their expectations are
written down in `tests/local/` instead.

- `NEXT` takes a list of variables here -- `NEXT J,I` closes both loops -- and
  the reference takes only one. Real Applesoft has always accepted the list,
  and programs written for real machines depend on it.
- After `ONERR` traps, `PEEK(218) + 256 * PEEK(219)` gives the line the error
  happened on. The reference gives the line `ONERR` was pointed at instead,
  which is of no use to the handler and is not what the machine did.

A third difference is not a divergence but a bug in the reference: `2 ^ 200`
hangs it. Overflow raises `?OVERFLOW` here, as it should.

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

Fifteen ship in `web/bundle`, and the IDE's Samples menu loads any of them in
one pick. Most want `-f`; see Speed below. From a shell:

    ./build/asoft -f -r web/bundle/MANDEL.BAS  # banded by escape time
    ./build/asoft -f -r web/bundle/JULIA.BAS   # a dendrite, same banding
    ./build/asoft -f -r web/bundle/FERN.BAS    # Barnsley, four affine maps
    ./build/asoft -f -r web/bundle/DRAGON.BAS  # turns taken from the step number
    ./build/asoft -f -r web/bundle/SIERP.BAS   # Sierpinski, by the chaos game
    ./build/asoft -f -r web/bundle/SPIRO.BAS   # hypotrochoids
    ./build/asoft -f -r web/bundle/MOIRE.BAS   # interference
    ./build/asoft -f -r web/bundle/CUBE.BAS    # a rotating wireframe
    ./build/asoft -f -r web/bundle/WIDE.BAS    # eighty columns, from PR#3
    ./build/asoft -r web/bundle/SNAKE.BAS      # I J K M to steer, Q to quit

Mandelbrot and Julia are banded by escape time into the four hi-res colours,
which is exactly the palette Apple II fractal art of the period had to work
with. The dragon curve walks itself twice -- once to find its size, once to
draw it scaled to fit -- so it fills the screen whatever order you give it.

Two of them are worth reading rather than only running. JULIA.BAS carries a
comment about why its row variable is `YY` and not `ZY0`: only two characters
of a name are significant, so `ZY0` *is* `ZY`, and each row would eat its own
starting value. CUBE.BAS explains why its eye is six units back rather than
four -- a rotated unit cube reaches SQR(3), and any closer the perspective
divide throws a corner off the screen and Applesoft stops with ILLEGAL
QUANTITY.

### The keyboard, for games

`GET` stops and waits, which is no use to anything that has to keep moving.
Programs of the period polled the keyboard soft switches instead, and those
work here: `PEEK(-16384)` returns the last key with bit 7 set while it is
unread, and `POKE -16368,0` acknowledges it. SNAKE.BAS is built on that, so it
keeps going while you decide where to turn.

Both front ends are tested against a real terminal for this, because a pipe
cannot show it: `tests/run_cli_keys.sh` drives the console build under a pty
and `tests/run_ide_keys.sh` drives the windowed one, and in each case a
running program has to see a key pressed while it runs. The game test asks for
Q, which ends differently from running into a wall, so QUIT rather than GAME
OVER is what proves the key arrived.

## Speed

The interpreter runs at the speed the machine ran at: about a thousand
statements a second, which puts `FOR I=1 TO 1000: NEXT` at a second and a
half, the same as an Apple II.

That is not nostalgia. Applesoft has no clock, so a program that wants to wait
counts to a number in a `FOR` loop, and how long that takes is a property of
the hardware. Run it flat out and a game crosses the screen before you can
press a key, and the delay constant that made it playable becomes meaningless
-- right for one machine, wrong for every other. Pacing the interpreter is
what lets a program's own timing loops mean the same thing on a laptop, under
DOSBox, and on a real DOS machine. SNAKE.BAS says `SP = 100` and that is
simply how long it waits, everywhere.

    ./build/asoft -f program.bas       # flat out
    ./build/asoft -s 5000 program.bas  # five thousand statements a second

Anything compute-bound wants `-f`: the Mandelbrot is a couple of hundred
thousand statements, which is four minutes at the machine's own pace, and was
four minutes on the machine too. `build/hgrdump` and `build/textdump` are
unthrottled already, since they are rendering a picture rather than pretending
to be a machine.

`build/hgrdump program.bas out.bmp [scale]` renders a program's graphics page
to a file, which is how the colour rules are checked without squinting at a
terminal.

While hi-res is up the DOS build prints nothing at all, because that is what
the Apple did: the prompt went to the text page, which was not the page on
screen. Type `TEXT` blind and press return to get back. The terminal build
does not do this -- it has only one screen, and hiding the prompt there would
just look broken.

## Licence

MIT; see [LICENSE](LICENSE).

Two things in the tree are not mine to relicense, and are worth knowing about
before publishing:

- `reference/ASOFT-watcom-reference.EXE` is the compiled binary this is
  checked against, from the original archive. Everything in `tools/capture`
  exists to run it and compare, so removing it costs the capture tests --
  they skip rather than fail without it.
- The corpus of third-party Applesoft that `tools/corpus.py` runs is *not* in
  this repository, deliberately: it is other people's programs under their own
  licences. Clone them wherever you like and point the tool at them.

The character set in `src/applefont.c` is authored to the Apple's 5x7-in-7x8
geometry rather than copied: the real character ROM is still Apple's.
