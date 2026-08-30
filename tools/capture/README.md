# Capture rig

The `src/` tree was reconstructed from a binary, not from notes. `ASOFT.EXE`
survived in the archive but its sources did not, so every behavioural claim in
this repository was measured by running that binary and writing down what it
did. This directory is how.

## How it works

DOSBox can redirect, so the rig never has to synthesise keystrokes:

```
ASOFT.EXE < SCRIPT.TXT > OUT.TXT
echo CAPTURE-DONE > DONE.TXT
```

`mkcapture.sh` packs a script into `capture.jsdos`; `capture.html` runs it under
js-dos, polls the virtual filesystem for `DONE.TXT`, then POSTs `OUT.TXT` back
to `server.py`, which drops it in `captured/`. `tests/run_capture.sh` replays
the same script through our own build and diffs the two transcripts.

Driving the emulator's keyboard was tried first and abandoned: js-dos exposes no
key-code table, and the WebGL canvas reads back blank to screenshots, so there
was no reliable way to see what had been typed. Redirection sidesteps both.

## Running one

```sh
./mkcapture.sh myscript.txt
python3 server.py 8130 &
# open http://localhost:8130/capture.html?name=myscript
```

The transcript lands in `captured/myscript.txt`.

## What the captures settled

Things that would otherwise have been guesses, each measured rather than
assumed:

- **LIST spacing.** Keywords print as `" KEYWORD "` and source spaces are
  dropped, so `30 IF X <> 0 THEN PRINT "HI"` lists as
  `30  IF X <  > 0 THEN  PRINT "HI"`. `REM` and `DATA` keep their tails
  verbatim, which is where `REM  APPLESOFT` gets its second space.
- **Number formatting.** Fixed point over `0.01 <= |v| < 1E9`, scientific
  outside it, nine significant digits, no leading zero, no leading space.
- **The prompt does not count.** `PRINT 1,2,3` puts the `2` at column 16, not
  17, so `]` is written outside the column model.
- **HTAB is off by two and prints nothing.** After `HTAB 10` the cursor reads
  11 and the next `PRINT` still wraps 29 characters later. This one turned out
  to be the reference build's own bug, not the ROM's: the genuine Applesoft
  ROM reads 9 there (see `tools/diff/`), so this build follows the hardware
  and no longer reproduces it.
- **Errors always break the line first**, even at column zero.
- **Memory accounting.** Program storage is byte-exact line by line; scalars
  cost 7 bytes, `DIM B(10)` costs 62, string literals cost nothing, `READ`
  copies its item.
- **The ONERR leak is 60 deep** — a 240-byte control stack losing 4 bytes per
  trapped error — and `CALL -3288` pops the frame.

## Known divergences from the reference

Deliberate, and not bugs here:

- `FRE(0)` after `TESTS.BAS` reads 35492 against the reference's 35491. Every
  allocation rule above was measured and matched; what remains is a single byte
  of collector residue inside a binary whose source is gone.
- `1E-5` parses as 1E-5. The reference stops after the `E`, leaving `-5` to be
  read as subtraction, and prints `-4`. The ROM accepts signed exponents.
- `LEFT$("ABCDEF",0)` returns the empty string; the reference raises
  `?ILLEGAL QUANTITY`.
- `LOAD`, `SAVE` and calling a `DEF FN` work here. The reference answers
  `?SYNTAX ERROR` to all three.
- Graphics plot into real page memory. The reference parses `PLOT` and `HPLOT`
  and does nothing, so `PEEK` cannot see what was drawn.
