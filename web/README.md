# Running it in the browser (js-dos)

## Layout

```
web/
  index.html          host page, loads js-dos v8 from the CDN
  ui-mockup.html      the proposed Turbo Vision layout, for review
  makebundle.sh       zips bundle/ into asoft.jsdos
  bundle/
    .jsdos/dosbox.conf   required by js-dos
    .jsdos/jsdos.json    title, mouse on
    ASOFT.EXE            <- you supply this
    *.BAS                preloaded programs
```

## Getting ASOFT.EXE

You need a real 16-bit DOS binary. Two routes, both without installing anything
16-bit on your own machine.

### A. Compile inside DOSBox (recommended, gets the Turbo Vision build)

Borland C++ 3.1 runs perfectly well under DOSBox, and Turbo Vision only compiles
under Borland anyway.

1. Make a second bundle containing BC++ 3.1, the Turbo Vision sources, and this
   repo's `src/` and `makefile.bc`.
2. Run it in DOSBox (local or js-dos), `make -f makefile.bc`.
3. Copy `ASOFT.EXE` out of the DOSBox C: mount into `web/bundle/`.

Once it builds, you never need to repeat this unless the source changes.

### B. Open Watcom, for the console build — already done

`web/bundle/ASOFT.EXE` in this tree is a real 16-bit MZ binary, cross-compiled with
Open Watcom 2.0 and run under DOSBox. `build-dos.sh` reproduces it:

```
curl -LO https://github.com/open-watcom/open-watcom-v2/releases/download/Current-build/ow-snapshot.tar.xz
mkdir -p ow && tar -xJf ow-snapshot.tar.xz -C ow
WATCOM=$PWD/ow ./build-dos.sh
```

Two flags matter. `-ml` (large model) makes every pointer far, which the 64K memory
image needs. `-k16384` raises the stack from Watcom's 4K default, which the
recursive-descent evaluator will otherwise blow through.

Under a 16-bit `int` the output is byte-identical to the host build, `FRE(0)` included
— so the MBF width handling and the memory model both hold up where it counts.

## Bundle and serve

```
cd web
./makebundle.sh
python3 -m http.server 8000
```

Open <http://localhost:8000/>. `file://` will not work — js-dos needs http.

## Notes on the DOSBox config

- `machine=svga_s3` so the 8x8 font (80x43) is available. Turbo Vision switches into
  it at startup.
- `cycles=20000` is plenty for a text-mode interpreter; `cycles=max` just burns
  battery. Raise it if a tight `FOR` loop feels slow.
- `nosound=true` — nothing makes sound yet. Change this when the bell and `PR#`
  land.
- The `[autoexec]` block mounts the bundle root as `C:` and runs `ASOFT.EXE`, so
  `LOAD TESTS.BAS` from the File menu finds the preloaded programs.
- Mouse is enabled and `autolock=false`, so Turbo Vision's menus, dialogs and window
  dragging work with the browser pointer without capturing it.
