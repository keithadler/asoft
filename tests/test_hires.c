/* The DOS display repaints only the columns a plot touched, which is what
 * makes it usable on real hardware. That optimisation is only safe if a
 * partial span agrees with the full row, so check exactly that. */
#include "../src/a2mem.h"
#include "../src/gfx.h"
#include "../src/hires.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;

/* Does hires_span(x0..x1) match hires_row over that range? */
static void compare(int y, int x0, int x1, const char *what)
{
    unsigned char full[HIRES_W], part[HIRES_W];
    int x;

    memset(full, 0xEE, sizeof(full));
    memset(part, 0xEE, sizeof(part));
    hires_row(y, full);
    hires_span(y, x0, x1, part);

    for (x = x0; x <= x1; x++) {
        if (part[x] == 0xEE) {
            printf("  FAIL  %s: span left x=%d unwritten\n", what, x);
            failures++;
            return;
        }
        if (part[x] != full[x]) {
            printf("  FAIL  %s: x=%d span says %d, full row says %d\n",
                   what, x, part[x], full[x]);
            failures++;
            return;
        }
    }
}

int main(void)
{
    int col, x;

    a2_init();
    gfx_reset();
    gfx_hgr();
    gfx_hcolor(3);

    /* A solid run, so most pixels are white and the ends are coloured. */
    for (x = 40; x < 120; x++)
        gfx_hplot(x, 10);
    /* Lone dots, which is where the colour rules actually bite. */
    gfx_hplot(200, 10);
    gfx_hplot(210, 10);
    gfx_hcolor(1);
    for (x = 150; x < 180; x++)
        gfx_hplot(x, 10);

    /* Every byte-sized span the display would repaint, exactly as
     * display_dos.c computes it. */
    for (col = 0; col < 40; col++) {
        char what[32];
        int x0 = col * 7 - 1, x1 = col * 7 + 7;
        if (x0 < 0) x0 = 0;
        if (x1 >= HIRES_W) x1 = HIRES_W - 1;
        sprintf(what, "byte col %d", col);
        compare(10, x0, x1, what);
    }

    /* And the awkward ones. */
    compare(10, 0, 0, "single pixel at left edge");
    compare(10, HIRES_W - 1, HIRES_W - 1, "single pixel at right edge");
    compare(10, 0, HIRES_W - 1, "whole row");
    compare(10, 139, 140, "across the middle");

    printf("test_hires: %s\n", failures ? "FAILED" : "ok");
    return failures ? 1 : 0;
}
