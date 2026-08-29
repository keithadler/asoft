#include "hires.h"
#include "a2mem.h"

/* Is the pixel at column x of this row lit? */
static int lit(a2addr base, int x)
{
    if (x < 0 || x >= HIRES_W)
        return 0;
    return (a2mem[base + x / 7] >> (x % 7)) & 1;
}

void hires_span(int y, int x0, int x1, unsigned char *out)
{
    a2addr base = gfx_hires_row(gfx_hpage(), y);
    int lo = x0 - 2, hi = x1 + 2;
    int x;

    if (lo < 0) lo = 0;
    if (hi >= HIRES_W) hi = HIRES_W - 1;

    for (x = lo; x <= hi; x++) {
        int palette;
        if (!lit(base, x)) {
            out[x] = PAL_HIRES + 0;              /* black */
            continue;
        }
        /* Two lit pixels side by side read as white however they were
         * coloured, because between them they fill a whole colour cycle. */
        if (lit(base, x - 1) || lit(base, x + 1)) {
            out[x] = PAL_HIRES + 3;              /* white */
            continue;
        }
        palette = (a2mem[base + x / 7] & 0x80) ? 1 : 0;
        if (x & 1)
            out[x] = (unsigned char)(PAL_HIRES + (palette ? 4 : 1));  /* orange : green */
        else
            out[x] = (unsigned char)(PAL_HIRES + (palette ? 5 : 2));  /* blue : violet */
    }

    /* A coloured dot occupies half a colour cycle, and the display cannot
     * show half a cycle: it fills the whole one. So a lone dot is two screen
     * pixels wide, which is why a green line drawn on every other column
     * comes out solid rather than dotted. Widening here rather than in the
     * plot keeps page memory honest -- PEEK still sees the single bit. */
    for (x = lo; x <= hi; x++) {
        int partner = x ^ 1;
        if (out[x] == PAL_HIRES + 0 || out[x] == PAL_HIRES + 3)
            continue;                            /* black and white stay put */
        if (partner >= lo && partner <= hi && out[partner] == PAL_HIRES + 0)
            out[partner] = out[x];
    }
}

void hires_row(int y, unsigned char *out)
{
    hires_span(y, 0, HIRES_W - 1, out);
}

int hires_pixel(int x, int y)
{
    unsigned char row[HIRES_W];
    if (x < 0 || x >= HIRES_W || y < 0 || y >= HIRES_H)
        return PAL_HIRES;
    hires_row(y, row);
    return row[x];
}

int lores_pixel(int x, int y)
{
    return PAL_LORES + gfx_scrn(x, y);
}
