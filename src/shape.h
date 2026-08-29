/* shape.h - Applesoft shape tables.
 *
 * A shape is a string of moves. Each byte packs three of them: two that can
 * plot as they go and a third that only moves, which is why shapes are so
 * compact and so awkward to author by hand. The bytes are walked in order
 * until a zero one ends the shape.
 *
 * The table itself is a small directory. Byte 0 is the number of shapes, byte
 * 1 is unused, and then a two-byte little-endian offset per shape, measured
 * from the start of the table. Programs POKE the whole thing into memory and
 * point $E8/$E9 at it.
 *
 * DRAW plots in the current HCOLOR; XDRAW inverts instead, which is how a
 * shape is erased by drawing it a second time in the same place. Every plot
 * that lands on an already-lit pixel bumps the collision counter at $EA, so a
 * program can tell whether two shapes overlap without reading the screen.
 */
#ifndef ASOFT_SHAPE_H
#define ASOFT_SHAPE_H

#include "a2mem.h"

/* Draw shape number n (1-based, as Applesoft counts them) at x, y. Returns 0
 * if the shape number is out of range for the table, which the caller turns
 * into ILLEGAL QUANTITY. rot and scale are read from zero page by the caller
 * so that a program POKEing $E7 or $F9 directly gets the same result. */
int shape_draw(a2addr table, int n, int x, int y, int rot, int scale, int xor_mode);

#endif
