# Host build. build-dos.sh cross-compiles the same sources for 16-bit DOS.
CC      ?= cc
# -Wdeclaration-after-statement is not in -Wall or -Wextra, and the DOS
# compiler is stricter than clang about it: without this, a declaration in
# the wrong place builds fine here and fails only when cross-compiling.
CFLAGS  ?= -std=c89 -Wall -Wextra -Wdeclaration-after-statement -O2
LDLIBS  ?= -lm

# display_dos.c and tui_dos.c are for the 16-bit DOS build only; see
# build-dos.sh. ide.c and the tui backends belong to the windowed front end,
# which is a separate binary from the plain console one.
SRC   := $(filter-out src/display_dos.c src/tui_%.c src/ide.c,$(wildcard src/*.c))
IDE   := src/ide.c src/tui_term.c src/tui_palette.c
TESTS := $(wildcard tests/test_*.c)
CORE  := $(filter-out src/main_%.c,$(SRC)) tests/host_stub.c

all: build/asoft build/asoft-ide build/layout build/hgrdump build/textdump

build/asoft: $(filter-out src/main_ide.c,$(SRC)) | build
	$(CC) $(CFLAGS) -I. -o $@ $(filter-out src/main_ide.c,$(SRC)) $(LDLIBS)

build/asoft-ide: $(filter-out src/main_%.c,$(SRC)) $(IDE) src/main_ide.c | build
	$(CC) $(CFLAGS) -I. -o $@ $(filter-out src/main_%.c,$(SRC)) $(IDE) src/main_ide.c $(LDLIBS)

build/layout: tools/layout.c $(filter-out src/main_%.c,$(SRC)) | build
	$(CC) $(CFLAGS) -I. -o $@ tools/layout.c $(filter-out src/main_%.c,$(SRC)) $(LDLIBS)

build/hgrdump: tools/hgrdump.c $(filter-out src/main_%.c,$(SRC)) | build
	$(CC) $(CFLAGS) -I. -o $@ tools/hgrdump.c $(filter-out src/main_%.c,$(SRC)) $(LDLIBS)

build/textdump: tools/textdump.c $(filter-out src/main_%.c,$(SRC)) | build
	$(CC) $(CFLAGS) -I. -o $@ tools/textdump.c $(filter-out src/main_%.c,$(SRC)) $(LDLIBS)

build:
	mkdir -p build

check: build/asoft build/asoft-ide
	@set -e; for t in $(TESTS); do \
	  n=$$(basename $$t .c); \
	  $(CC) $(CFLAGS) -I. -o build/$$n $$t $(CORE) $(LDLIBS); \
	  ./build/$$n; \
	done
	@./tests/run_capture.sh tests 'FRE(0)=35508'
	@./tests/run_capture.sh wide
	@./tests/run_capture.sh remcolon
	@./tests/run_capture.sh lowercase
	@./tests/run_capture.sh mbfacc
	@./tests/run_capture.sh immfor
	@./tests/run_capture.sh qmark
	@./tests/run_capture.sh attoamb
	@./tests/run_capture.sh atthen
	@./tests/run_capture.sh spacedkw
	@./tests/run_capture.sh dimscalar
	@./tests/run_capture.sh errors
	@./tests/run_capture.sh inputget
	@./tests/run_local.sh nextlist
	@./tests/run_local.sh apple
	@./tests/run_cli_keys.sh
	@./tests/run_ide.sh
	@./tests/run_ide_edit.sh
	@./tests/run_ide_keys.sh
	@./tests/run_ide_dos.sh
	@./tests/run_mouse_dos.sh

clean:
	rm -rf build

.PHONY: all check clean
