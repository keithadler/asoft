#include "hires.h"
#include "a2mem.h"

/* The artifact rules, over a row of 40 bytes. Hi-res and text both end up
 * here: they were the same 280x192 field at the same dot clock, so a
 * character picked up colour fringing exactly as a plotted dot did.
 *
 * The bits are unpacked once into a byte per pixel first. Asking "is this
 * pixel lit" straight from the packed bytes costs a division and a modulo
 * per question, three questions per pixel, and a 16-bit DOS build under an
 * emulator repainting a whole screen through this after every statement
 * spent most of its time dividing. */
void artifact_span(const unsigned char *row, int x0, int x1, unsigned char *out)
{
    /* lit[x + 1] is pixel x; the two extra cells are the dark borders. */
    unsigned char lit[HIRES_W + 2];
    int lo = x0 - 2, hi = x1 + 2;
    int b0, b1, b, x, bi, cnt;

    if (lo < 0) lo = 0;
    if (hi >= HIRES_W) hi = HIRES_W - 1;

    b0 = (lo > 0 ? lo - 1 : 0) / 7;
    b1 = (hi < HIRES_W - 1 ? hi + 1 : HIRES_W - 1) / 7;
    for (b = b0; b <= b1; b++) {
        unsigned char byte = row[b];
        unsigned char *p = &lit[b * 7 + 1];
        int i;
        for (i = 0; i < 7; i++)
            p[i] = (unsigned char)((byte >> i) & 1);
    }
    lit[0] = 0;
    lit[HIRES_W + 1] = 0;
    if (b0 > 0) lit[b0 * 7] = 0;                    /* left of the unpacked run */
    if (b1 < 39) lit[(b1 + 1) * 7 + 1] = 0;         /* right of it */

    bi = lo / 7;
    cnt = lo - bi * 7;
    for (x = lo; x <= hi; x++) {
        int palette;
        if (!lit[x + 1]) {
            out[x] = PAL_HIRES + 0;              /* black */
        } else if (lit[x] || lit[x + 2]) {
            /* Two lit pixels side by side read as white however they were
             * coloured, because between them they fill a whole colour cycle. */
            out[x] = PAL_HIRES + 3;              /* white */
        } else {
            palette = (row[bi] & 0x80) ? 1 : 0;
            if (x & 1)
                out[x] = (unsigned char)(PAL_HIRES + (palette ? 4 : 1));  /* orange : green */
            else
                out[x] = (unsigned char)(PAL_HIRES + (palette ? 5 : 2));  /* blue : violet */
        }
        if (++cnt == 7) { cnt = 0; bi++; }
    }

    /* A coloured dot occupies half a colour cycle, and the display cannot
     * show half a cycle: it fills the whole one. So a lone dot is two screen
     * pixels wide, which is why a green line drawn on every other column
     * comes out solid rather than dotted, and why white text comes out
     * fringed rather than white. */
    for (x = lo; x <= hi; x++) {
        int partner = x ^ 1;
        if (out[x] == PAL_HIRES + 0 || out[x] == PAL_HIRES + 3)
            continue;                            /* black and white stay put */
        if (partner >= lo && partner <= hi && out[partner] == PAL_HIRES + 0)
            out[partner] = out[x];
    }
}

void hires_span(int y, int x0, int x1, unsigned char *out)
{
    /* The page the switches have on the display, which is not always the
     * one HPLOT is drawing on. */
    artifact_span(&a2mem[gfx_hires_row(gfx_disp_hpage(), y)], x0, x1, out);
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
