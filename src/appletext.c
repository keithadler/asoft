#include "appletext.h"
#include "applefont.h"
#include "hires.h"

#include <string.h>

/* One scan line of the text screen as 40 bytes, in hi-res byte order: seven
 * pixels to a byte, bit 0 leftmost. A character's five dots occupy the first
 * five, and the last two are the gap to the next character. */
static void scanline_bits(const char *cells, int y, unsigned char *row)
{
    int textrow = y / APPLE_CELL_H;
    int line = y % APPLE_CELL_H;
    int col;

    for (col = 0; col < ATEXT_COLS; col++) {
        unsigned char ch = (unsigned char)cells[textrow * ATEXT_COLS + col];
        unsigned char bits, out = 0;
        int i;

        if (ch < APPLE_FONT_FIRST || ch >= APPLE_FONT_FIRST + APPLE_FONT_CHARS)
            ch = ' ';
        bits = apple_font[ch - APPLE_FONT_FIRST][line];
        /* The glyph stores bit 4 leftmost; the display wants bit 0 leftmost. */
        for (i = 0; i < 5; i++)
            if ((bits >> (4 - i)) & 1)
                out |= (unsigned char)(1 << i);
        row[col] = out;
    }
}

void appletext_row(const char *cells, int y, unsigned char *out)
{
    unsigned char row[ATEXT_COLS];
    scanline_bits(cells, y, row);
    artifact_span(row, 0, ATEXT_W - 1, out);
}

void appletext_render(const char *cells, unsigned char *out)
{
    int y;
    for (y = 0; y < ATEXT_H; y++)
        appletext_row(cells, y, out + (long)y * ATEXT_W);
}
