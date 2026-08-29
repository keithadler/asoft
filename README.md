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
