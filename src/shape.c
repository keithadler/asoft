#include "shape.h"
#include "gfx.h"

/* Directions, in the order the hardware numbered them. */
#define DIR_UP    0
#define DIR_RIGHT 1
#define DIR_DOWN  2
#define DIR_LEFT  3

static void step(int dir, int *x, int *y)
{
    switch (dir) {
    case DIR_UP:    (*y)--; break;
    case DIR_RIGHT: (*x)++; break;
    case DIR_DOWN:  (*y)++; break;
    default:        (*x)--; break;
    }
}

static void bump_collision(void)
{
    a2mem[ZP_COLLISION] = (unsigned char)(a2mem[ZP_COLLISION] + 1);
}

/* One vector: plot where we are if asked, then move. Scale repeats the whole
 * thing, so a shape at SCALE=4 is four times the size rather than a dotted
 * version of itself. */
static void vector(int plot, int dir, int *x, int *y, int scale, int xor_mode)
{
    int i;
    for (i = 0; i < scale; i++) {
        /* A shape walked off the edge keeps walking -- the pen position still
         * matters when it comes back -- but nothing is drawn while it is out
         * there. HPLOT rejects bad coordinates outright; DRAW cannot, because
         * only the starting point is the program's fault. */
        if (plot && *x >= 0 && *x < 280 && *y >= 0 && *y < 192) {
            if (gfx_hscrn(*x, *y))
                bump_collision();
            if (xor_mode)
                gfx_hxor(*x, *y);
            else
                gfx_hplot(*x, *y);
        }
        step(dir, x, y);
    }
}

int shape_draw(a2addr table, int n, int x, int y, int rot, int scale, int xor_mode)
{
    int count = a2mem[table];
    int quadrant;
    a2addr p;

    if (n < 1 || n > count)
        return 0;
    if (scale < 1)
        scale = 1;

    /* ROT runs 0..63 for a full turn. The ROM only turns cleanly at the
     * quadrants; in between it walks a distorted version of the shape that
     * depends on the scale. This rounds to the nearest quadrant instead,
     * which is right on the multiples of 16 that programs actually use and
     * an approximation elsewhere. */
    quadrant = ((rot & 63) + 8) / 16;
    quadrant &= 3;

    p = (a2addr)(table + a2mem[table + 2 + (n - 1) * 2] +
                 ((a2addr)a2mem[table + 3 + (n - 1) * 2] << 8));

    for (;;) {
        unsigned char b = a2mem[p++];
        int dir;

        if (b == 0)
            return 1;                       /* a zero byte ends the shape */

        /* Section A: bit 2 plots, bits 0-1 steer. */
        dir = (b & 3);
        vector(b & 0x04, (dir + quadrant) & 3, &x, &y, scale, xor_mode);

        /* If nothing is left above bit 2 the byte is finished. Without this
         * the trailing zeros would be read as three moves upward. */
        if ((b & 0xF8) == 0)
            continue;

        /* Section B: bit 5 plots, bits 3-4 steer. */
        dir = (b >> 3) & 3;
        vector(b & 0x20, (dir + quadrant) & 3, &x, &y, scale, xor_mode);

        if ((b & 0xC0) == 0)
            continue;

        /* Section C only moves; there is no room left for a plot bit. */
        dir = (b >> 6) & 3;
        vector(0, (dir + quadrant) & 3, &x, &y, scale, xor_mode);
    }
}
