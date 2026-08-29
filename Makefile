# Host build. build-dos.sh cross-compiles the same sources for 16-bit DOS.
CC      ?= cc
CFLAGS  ?= -std=c89 -Wall -Wextra -O2
LDLIBS  ?= -lm

SRC   := $(wildcard src/*.c)
TESTS := $(wildcard tests/test_*.c)
CORE  := $(filter-out src/main_%.c,$(SRC)) tests/host_stub.c

all: build/asoft build/layout

build/asoft: $(SRC) | build
	$(CC) $(CFLAGS) -I. -o $@ $(SRC) $(LDLIBS)

build/layout: tools/layout.c $(filter-out src/main_%.c,$(SRC)) | build
	$(CC) $(CFLAGS) -I. -o $@ tools/layout.c $(filter-out src/main_%.c,$(SRC)) $(LDLIBS)

build:
	mkdir -p build

check: build/asoft
	@set -e; for t in $(TESTS); do \
	  n=$$(basename $$t .c); \
	  $(CC) $(CFLAGS) -I. -o build/$$n $$t $(CORE) $(LDLIBS); \
	  ./build/$$n; \
	done
	@./tests/run_capture.sh tests 'FRE(0)=35491'
	@./tests/run_capture.sh wide

clean:
	rm -rf build

.PHONY: all check clean
