# Host build. build-dos.sh cross-compiles the same sources for 16-bit DOS.
CC      ?= cc
CFLAGS  ?= -std=c89 -Wall -Wextra -O2
LDLIBS  ?= -lm

# display_dos.c and tui_dos.c are for the 16-bit DOS build only; see
# build-dos.sh. ide.c and the tui backends belong to the windowed front end,
# which is a separate binary from the plain console one.
SRC   := $(filter-out src/display_dos.c src/tui_%.c src/ide.c,$(wildcard src/*.c))
IDE   := src/ide.c src/tui_term.c
TESTS := $(wildcard tests/test_*.c)
CORE  := $(filter-out src/main_%.c,$(SRC)) tests/host_stub.c

all: build/asoft build/asoft-ide build/layout build/hgrdump

build/asoft: $(filter-out src/main_ide.c,$(SRC)) | build
	$(CC) $(CFLAGS) -I. -o $@ $(filter-out src/main_ide.c,$(SRC)) $(LDLIBS)

build/asoft-ide: $(filter-out src/main_%.c,$(SRC)) $(IDE) src/main_ide.c | build
	$(CC) $(CFLAGS) -I. -o $@ $(filter-out src/main_%.c,$(SRC)) $(IDE) src/main_ide.c $(LDLIBS)

build/layout: tools/layout.c $(filter-out src/main_%.c,$(SRC)) | build
	$(CC) $(CFLAGS) -I. -o $@ tools/layout.c $(filter-out src/main_%.c,$(SRC)) $(LDLIBS)

build/hgrdump: tools/hgrdump.c $(filter-out src/main_%.c,$(SRC)) | build
	$(CC) $(CFLAGS) -I. -o $@ tools/hgrdump.c $(filter-out src/main_%.c,$(SRC)) $(LDLIBS)

build:
	mkdir -p build

check: build/asoft build/asoft-ide
	@set -e; for t in $(TESTS); do \
	  n=$$(basename $$t .c); \
	  $(CC) $(CFLAGS) -I. -o build/$$n $$t $(CORE) $(LDLIBS); \
	  ./build/$$n; \
	done
	@./tests/run_capture.sh tests 'FRE(0)=35491'
	@./tests/run_capture.sh wide
	@./tests/run_capture.sh remcolon
	@./tests/run_ide.sh
	@./tests/run_ide_dos.sh

clean:
	rm -rf build

.PHONY: all check clean
