/* Shape tables: the vector packing, scale, rotation, XDRAW and the collision
 * counter. Drawn shapes are checked pixel by pixel rather than by eye. */
#include "../src/a2mem.h"
#include "../src/gfx.h"
#include "../src/shape.h"
#include <stdio.h>

static int failures = 0;
#define TABLE 0x6000

static void check(const char *what, int got, int want)
{
    if (got != want) {
        printf("  FAIL  %s: got %d, want %d\n", what, got, want);
        failures++;
    }
}

static void pixel(const char *what, int x, int y, int want)
{
    int got = gfx_hscrn(x, y);
    if (got != want) {
        printf("  FAIL  %s: pixel (%d,%d) is %d, want %d\n", what, x, y, got, want);
        failures++;
    }
}

/* Count everything lit, so "drew nothing" and "drew too much" both show up. */
static int lit_count(void)
{
    int x, y, n = 0;
    for (y = 0; y < 192; y++)
        for (x = 0; x < 280; x++)
            n += gfx_hscrn(x, y);
    return n;
}

/* A table holding one shape: four plotted moves to the right, then stop.
 * 0x0D is two sections: A = plot right, B = plot right. */
static void one_shape(const unsigned char *body, int len)
{
    int i;
    a2mem[TABLE + 0] = 1;          /* one shape */
    a2mem[TABLE + 1] = 0;
    a2mem[TABLE + 2] = 4;          /* offset to it, low byte */
    a2mem[TABLE + 3] = 0;
    for (i = 0; i < len; i++)
        a2mem[TABLE + 4 + i] = body[i];
}

int main(void)
{
    /* Two plotted moves per byte, twice over, then a zero to end the shape.
     * 0x2D is section A = plot right (bit 2 set, direction 1) and section B =
     * plot right (bit 5 set, direction 1). The plot bits sit in different
     * places for the two sections, which is easy to get wrong: A plots with
     * 0x04 and B with 0x20. */
    static const unsigned char four_right[] = { 0x2D, 0x2D, 0x00 };
    /* Plot up twice: direction 0 in both sections, both plotting. */
    static const unsigned char two_up[] = { 0x24, 0x00 };

    a2_init();
    gfx_reset();
    gfx_hgr();
    gfx_hcolor(3);                 /* white: lights every column */

    /* --- the basics ---------------------------------------------------- */
    one_shape(four_right, sizeof(four_right));
    check("shape number 0 is rejected", shape_draw(TABLE, 0, 10, 10, 0, 1, 0), 0);
    check("shape past the end is rejected", shape_draw(TABLE, 2, 10, 10, 0, 1, 0), 0);
    check("shape 1 draws", shape_draw(TABLE, 1, 10, 10, 0, 1, 0), 1);

    pixel("four right", 10, 10, 1);
    pixel("four right", 11, 10, 1);
    pixel("four right", 12, 10, 1);
    pixel("four right", 13, 10, 1);
    pixel("four right stops", 14, 10, 0);
    check("exactly four pixels", lit_count(), 4);

    /* --- scale ---------------------------------------------------------- */
    gfx_hgr();
    shape_draw(TABLE, 1, 10, 20, 0, 3, 0);
    check("scale 3 triples the length", lit_count(), 12);
    pixel("scale 3 reaches", 21, 20, 1);
    pixel("scale 3 stops", 22, 20, 0);

    /* --- rotation ------------------------------------------------------- */
    gfx_hgr();
    shape_draw(TABLE, 1, 50, 50, 16, 1, 0);   /* a quarter turn: right -> down */
    pixel("rot 16 turns right into down", 50, 51, 1);
    pixel("rot 16 does not go right", 51, 50, 0);
    check("rot 16 draws the same count", lit_count(), 4);

    gfx_hgr();
    shape_draw(TABLE, 1, 50, 50, 32, 1, 0);   /* half turn: right -> left */
    pixel("rot 32 turns right into left", 49, 50, 1);
    check("rot 32 draws the same count", lit_count(), 4);

    /* --- up, and the packing rule that makes it possible ---------------- */
    gfx_hgr();
    one_shape(two_up, sizeof(two_up));
    shape_draw(TABLE, 1, 30, 30, 0, 1, 0);
    pixel("plot up", 30, 30, 1);
    pixel("plot up", 30, 29, 1);
    check("two pixels up", lit_count(), 2);

    /* --- XDRAW erases what it drew -------------------------------------- */
    gfx_hgr();
    one_shape(four_right, sizeof(four_right));
    shape_draw(TABLE, 1, 60, 60, 0, 2, 1);
    check("xdraw lights pixels", lit_count(), 8);
    shape_draw(TABLE, 1, 60, 60, 0, 2, 1);
    check("xdraw again clears them", lit_count(), 0);

    /* --- the collision counter ------------------------------------------ */
    gfx_hgr();
    a2mem[ZP_COLLISION] = 0;
    shape_draw(TABLE, 1, 70, 70, 0, 1, 0);
    check("no collisions on empty screen", a2mem[ZP_COLLISION], 0);
    shape_draw(TABLE, 1, 70, 70, 0, 1, 0);
    check("every pixel collides the second time", a2mem[ZP_COLLISION], 4);

    /* --- off the edge ---------------------------------------------------- */
    gfx_hgr();
    shape_draw(TABLE, 1, 278, 100, 0, 1, 0);   /* runs off the right */
    check("clipped, not wrapped", lit_count(), 2);
    pixel("nothing wrapped to the next row", 0, 101, 0);

    printf("test_shape: %s\n", failures ? "FAILED" : "ok");
    return failures ? 1 : 0;
}
