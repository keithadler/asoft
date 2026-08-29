/* textdump - run a program and render the text screen the way the display
 * showed it: 5x7 dots in a 7x8 cell on the 280x192 field, through the
 * artifact rules. White text comes out fringed, which is what an Apple on a
 * colour monitor looked like and what a PC text mode cannot reproduce.
 *
 * usage: textdump program.bas out.bmp [scale]
 */
#include "../src/a2mem.h"
#include "../src/appletext.h"
#include "../src/gfx.h"
#include "../src/host.h"
#include "../src/interp.h"
#include "../src/screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char cells[ATEXT_ROWS][ATEXT_COLS];
static int row, col;

static void scroll_up(void)
{
    int r;
    for (r = 0; r < ATEXT_ROWS - 1; r++)
        memcpy(cells[r], cells[r + 1], ATEXT_COLS);
    memset(cells[ATEXT_ROWS - 1], ' ', ATEXT_COLS);
}

static void newline(void)
{
    col = 0;
    if (row < ATEXT_ROWS - 1) row++;
    else scroll_up();
}

static void sink(char ch)
{
    if (ch == '\f') {
        int r;
        for (r = 0; r < ATEXT_ROWS; r++)
            memset(cells[r], ' ', ATEXT_COLS);
        row = col = 0;
        return;
    }
    if (ch == '\n') { newline(); return; }
    if (col >= ATEXT_COLS) newline();
    cells[row][col++] = ch;
}

int host_getline(char *b, int m) { (void)b; (void)m; return 0; }
int host_getkey(void) { return 0; }
int host_echoes(void) { return 0; }
int host_pollkey(void) { return 0; }
int host_break(void) { return 0; }

static void put16(FILE *f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }
static void put32(FILE *f, unsigned long v)
{
    fputc((int)(v & 255), f); fputc((int)((v >> 8) & 255), f);
    fputc((int)((v >> 16) & 255), f); fputc((int)((v >> 24) & 255), f);
}

int main(int argc, char **argv)
{
    static unsigned char pix[ATEXT_H][ATEXT_W];
    int scale = (argc > 3) ? atoi(argv[3]) : 3;
    int x, y, pad, r;
    FILE *f;

    if (argc < 3) {
        fprintf(stderr, "usage: %s program.bas out.bmp [scale]\n", argv[0]);
        return 2;
    }
    if (scale < 1) scale = 1;

    for (r = 0; r < ATEXT_ROWS; r++)
        memset(cells[r], ' ', ATEXT_COLS);

    scr_init(sink);
    it_init();
    if (!it_load(argv[1])) { fprintf(stderr, "cannot load %s\n", argv[1]); return 1; }
    it_line("RUN");

    appletext_render(&cells[0][0], &pix[0][0]);

    f = fopen(argv[2], "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", argv[2]); return 1; }
    pad = (4 - (ATEXT_W * scale * 3) % 4) % 4;
    fputc('B', f); fputc('M', f);
    put32(f, 54UL + (unsigned long)(ATEXT_W * scale * 3 + pad) * (unsigned long)(ATEXT_H * scale));
    put32(f, 0); put32(f, 54);
    put32(f, 40); put32(f, (unsigned long)(ATEXT_W * scale));
    put32(f, (unsigned long)(ATEXT_H * scale));
    put16(f, 1); put16(f, 24); put32(f, 0); put32(f, 0);
    put32(f, 2835); put32(f, 2835); put32(f, 0); put32(f, 0);

    for (y = ATEXT_H * scale - 1; y >= 0; y--) {
        for (x = 0; x < ATEXT_W * scale; x++) {
            const unsigned char *c = gfx_palette[pix[y / scale][x / scale]];
            fputc(c[2], f); fputc(c[1], f); fputc(c[0], f);
        }
        for (x = 0; x < pad; x++) fputc(0, f);
    }
    fclose(f);
    printf("wrote %s (%dx%d)\n", argv[2], ATEXT_W * scale, ATEXT_H * scale);
    return 0;
}
