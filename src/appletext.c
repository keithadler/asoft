#include "appletext.h"
#include "applefont.h"
#include "gfx.h"
#include "hires.h"

#include <string.h>

/* The seven bits of one scan line of one cell, bit 0 leftmost, from a glyph
 * code the font knows about. A character's five dots occupy the first five,
 * and the last two are the gap to the next character. */
static unsigned char glyph_bits(unsigned char ch, int line)
{
    unsigned char bits, out = 0;
    int i;

    if (ch < APPLE_FONT_FIRST || ch >= APPLE_FONT_FIRST + APPLE_FONT_CHARS)
        ch = ' ';
    bits = apple_font[ch - APPLE_FONT_FIRST][line];
    /* The glyph stores bit 4 leftmost; the display wants bit 0 leftmost. */
    for (i = 0; i < 5; i++)
        if ((bits >> (4 - i)) & 1)
            out |= (unsigned char)(1 << i);
    return out;
}

/* One scan line of the text screen as 40 bytes, in hi-res byte order. */
static void scanline_bits(const char *cells, int y, unsigned char *row)
{
    int textrow = y / APPLE_CELL_H;
    int line = y % APPLE_CELL_H;
    int col;

    for (col = 0; col < ATEXT_COLS; col++)
        row[col] = glyph_bits((unsigned char)cells[textrow * ATEXT_COLS + col], line);
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

/* A byte of the page, as the character generator read it: the low six bits
 * pick one of sixty-four glyphs, so $C1 and $01 and $41 are all an "A", and
 * bit 7 decides whether the cell is inverted. Lower case ($E0-$FF) lands on
 * the symbols, which is what a II+ showed for it. */
static unsigned char page_bits(unsigned char b, int line)
{
    unsigned char glyph;
    unsigned char bits;

    /* Lower case ($60-$7F in either half) has no glyph of its own in the
     * sixty-four; a II+ showed symbols for it, and a IIe the letters. The
     * PC text mode here shows the letters, so the picture does too. */
    if ((b & 0x60) == 0x60)
        b = (unsigned char)(b - 0x20);
    glyph = b & 0x3F;
    if (glyph < 0x20)
        glyph += 0x40;
    bits = glyph_bits(glyph, line);
    if (!(b & 0x80))
        bits ^= 0x7F;
    return bits;
}

void appletext_wide_row(const unsigned char *page, int y, unsigned char *out)
{
    int textrow = y / APPLE_CELL_H;
    int line = y % APPLE_CELL_H;
    int col, i;

    for (col = 0; col < 80; col++) {
        unsigned char b = page[textrow * 80 + col];
        unsigned char bits = page_bits(b, line);     /* seven bits, bit 0 leftmost */
        unsigned char *o = out + col * 4;
        int inverse = !(b & 0x80);
        /* Columns 0, 2 and 4 of the five, then the gap. */
        for (i = 0; i < 3; i++)
            o[i] = (unsigned char)(((bits >> (i * 2)) & 1) ? PAL_HIRES + 3 : PAL_HIRES + 0);
        o[3] = (unsigned char)(inverse ? PAL_HIRES + 3 : PAL_HIRES + 0);
    }
}

void appletext_page_row(int y, unsigned char *out)
{
    unsigned char row[ATEXT_COLS];
    int textrow = y / APPLE_CELL_H;
    int line = y % APPLE_CELL_H;
    int col;

    for (col = 0; col < ATEXT_COLS; col++)
        row[col] = page_bits(gfx_text_get(textrow, col), line);
    artifact_span(row, 0, ATEXT_W - 1, out);
}
