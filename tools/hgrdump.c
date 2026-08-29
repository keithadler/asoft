/* hgrdump - run a program and write whatever it drew to a BMP.
 *
 * The DOS build shows graphics on VGA and the console build draws them with
 * terminal half-blocks, but neither is much use for checking that the colour
 * rules are right. This renders the page to a file instead, at whatever
 * integer scale you ask for.
 *
 * usage: hgrdump program.bas out.bmp [scale]
 */
#include "../src/a2mem.h"
#include "../src/display.h"
#include "../src/gfx.h"
#include "../src/hires.h"
#include "../src/host.h"
#include "../src/interp.h"
#include "../src/screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sink(char ch) { (void)ch; }
int host_getline(char *buf, int max) { (void)buf; (void)max; return 0; }
int host_getkey(void) { return 0; }
int host_break(void) { return 0; }

static void put16(FILE *f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }
static void put32(FILE *f, unsigned long v)
{
    fputc((int)(v & 255), f); fputc((int)((v >> 8) & 255), f);
    fputc((int)((v >> 16) & 255), f); fputc((int)((v >> 24) & 255), f);
}

int main(int argc, char **argv)
{
    int scale = (argc > 3) ? atoi(argv[3]) : 3;
    int w, h, x, y, pad;
    FILE *f;

    if (argc < 3) {
        fprintf(stderr, "usage: %s program.bas out.bmp [scale]\n", argv[0]);
        return 2;
    }
    if (scale < 1) scale = 1;

    scr_init(sink);
    it_init();
    disp_init();
    gfx_on_change(disp_touch);

    if (!it_load(argv[1])) { fprintf(stderr, "cannot load %s\n", argv[1]); return 1; }
    it_line("RUN");

    /* Output goes nowhere here, so an error would otherwise be silent and the
     * picture would just come out wrong. */
    if (a2mem[ZP_ERRNUM] != 0)
        fprintf(stderr, "warning: stopped with error %d at line %u\n",
                a2mem[ZP_ERRNUM],
                (unsigned)(a2mem[ZP_ERRLIN] | (a2mem[ZP_ERRLIN + 1] << 8)));

    if (gfx_mode() == GFX_TEXT) {
        fprintf(stderr, "the program left the display in text mode; nothing to dump\n");
        return 1;
    }

    if (gfx_mode() == GFX_HIRES) { w = HIRES_W; h = HIRES_H; }
    else                         { w = LORES_W; h = LORES_H; }

    f = fopen(argv[2], "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", argv[2]); return 1; }

    /* 24-bit BMP: rows are bottom-up and padded to a multiple of four. */
    pad = (4 - (w * scale * 3) % 4) % 4;
    fputc('B', f); fputc('M', f);
    put32(f, 54UL + (unsigned long)(w * scale * 3 + pad) * (unsigned long)(h * scale));
    put32(f, 0); put32(f, 54);
    put32(f, 40); put32(f, (unsigned long)(w * scale)); put32(f, (unsigned long)(h * scale));
    put16(f, 1); put16(f, 24); put32(f, 0); put32(f, 0);
    put32(f, 2835); put32(f, 2835); put32(f, 0); put32(f, 0);

    for (y = h * scale - 1; y >= 0; y--) {
        unsigned char row[HIRES_W];
        int sy = y / scale;
        if (gfx_mode() == GFX_HIRES)
            hires_row(sy, row);
        else
            for (x = 0; x < w; x++) row[x] = (unsigned char)lores_pixel(x, sy);
        for (x = 0; x < w * scale; x++) {
            const unsigned char *c = gfx_palette[row[x / scale]];
            fputc(c[2], f); fputc(c[1], f); fputc(c[0], f);   /* BMP is BGR */
        }
        for (x = 0; x < pad; x++) fputc(0, f);
    }
    fclose(f);
    printf("wrote %s (%dx%d, scale %d)\n", argv[2], w * scale, h * scale, scale);
    return 0;
}
