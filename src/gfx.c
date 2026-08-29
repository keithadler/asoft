#include "gfx.h"
#include "bugs.h"

#include <string.h>

static int mode = GFX_TEXT;
static int color;             /* lo-res colour 0..15 */
static int hcolor;            /* hi-res colour 0..7 */
static a2addr hpage = HIRES_PAGE1;
static int last_x, last_y;

/* HCOLOR does not pick a colour so much as a bit pattern. The ROM keeps a
 * byte per colour and plots the bit that lands under x, which is why green
 * only lights odd columns and violet only even ones: a single dot of either
 * is invisible half the time, and that is the hardware being honest rather
 * than a bug. Bit 7 selects the palette pair (green/violet or orange/blue).
 *
 *   0 black1 $00   1 green  $2A   2 violet $55   3 white1 $7F
 *   4 black2 $80   5 orange $AA   6 blue   $D5   7 white2 $FF
 */
/* Rather than store the ROM's byte and rotate it across byte boundaries, ask
 * directly whether this colour lights this column. It comes to the same thing
 * at the left of a byte and stays right across the whole row, which the raw
 * byte does not: 0x55 lights even columns in byte 0 and odd ones in byte 1. */
static int hcolor_lights(int c, int x)
{
    switch (c & 3) {
    case 0:  return 0;              /* black */
    case 1:  return (x & 1);        /* green, orange: odd columns */
    case 2:  return !(x & 1);       /* violet, blue: even columns */
    default: return 1;              /* white */
    }
}

/* Apple's palette, near enough. 0..15 lo-res, 16..21 hi-res. */
const unsigned char gfx_palette[PAL_SIZE][3] = {
    {   0,   0,   0 }, { 227,  30,  96 }, {  96,  78, 189 }, { 255,  68, 253 },
    {   0, 163,  96 }, { 156, 156, 156 }, {  20, 207, 253 }, { 208, 195, 255 },
    {  96, 114,   3 }, { 255, 106,  60 }, { 156, 156, 156 }, { 255, 160, 208 },
    {  20, 245,  60 }, { 208, 221, 141 }, { 114, 255, 208 }, { 255, 255, 255 },
    /* hi-res: black, green, violet, white, orange, blue */
    {   0,   0,   0 }, {  20, 245,  60 }, { 255,  68, 253 }, { 255, 255, 255 },
    { 255, 106,  60 }, {  20, 207, 253 }
};

static gfx_hook change_hook;

void gfx_on_change(gfx_hook h) { change_hook = h; }

/* Tell the display one byte of page memory moved. Plotting a pixel is one
 * call; clearing a screen reports itself as a mode change instead. */
static void gfx_touch(a2addr a)
{
    if (change_hook)
        change_hook(a);
}

void gfx_reset(void)
{
    mode = GFX_TEXT;
    color = 0;
    hcolor = 3;
    hpage = HIRES_PAGE1;
    last_x = last_y = 0;
}

int gfx_mode(void) { return mode; }
int gfx_get_color(void) { return color; }
int gfx_get_hcolor(void) { return hcolor; }
a2addr gfx_hpage(void) { return hpage; }

int gfx_hscrn(int x, int y)
{
    a2addr a;
    if (x < 0 || x >= 280 || y < 0 || y >= 192)
        return 0;
    a = gfx_hires_row(hpage, y) + x / 7;
    return (a2mem[a] >> (x % 7)) & 1;
}

void gfx_hxor(int x, int y)
{
    a2addr a;
    if (x < 0 || x >= 280 || y < 0 || y >= 192)
        return;
    a = gfx_hires_row(hpage, y) + x / 7;
    a2mem[a] ^= (unsigned char)(1 << (x % 7));
    last_x = x;
    last_y = y;
    gfx_touch(a);
}

a2addr gfx_text_row(int row)
{
    return (a2addr)(LORES_PAGE1 + (row & 7) * 0x80 + (row >> 3) * 0x28);
}

a2addr gfx_hires_row(a2addr page, int y)
{
    return (a2addr)(page + (y & 7) * 0x400 + ((y >> 3) & 7) * 0x80 + (y >> 6) * 0x28);
}

static void clear_lores(void)
{
    int row;
    /* GR clears the top 40 rows of pixels and leaves the bottom four text
     * lines alone, which is why programs print prompts under the picture. */
    for (row = 0; row < 20; row++)
        memset(&a2mem[gfx_text_row(row)], 0, 40);
}

static void clear_hires(a2addr page)
{
    int y;
    for (y = 0; y < 192; y++)
        memset(&a2mem[gfx_hires_row(page, y)], 0, 40);
}

void gfx_gr(void)   { mode = GFX_LORES; color = 0; clear_lores(); gfx_touch(0); }
void gfx_text(void) { mode = GFX_TEXT; gfx_touch(0); }

void gfx_hgr(void)
{
    mode = GFX_HIRES;
    hpage = HIRES_PAGE1;
    clear_hires(hpage);
    last_x = last_y = 0;
    gfx_touch(0);
}

void gfx_hgr2(void)
{
    mode = GFX_HIRES;
    hpage = HIRES_PAGE2;
    clear_hires(hpage);
    last_x = last_y = 0;
    gfx_touch(0);
}

void gfx_color(int c)  { color = c & 15; }
void gfx_hcolor(int c) { hcolor = c & 7; }

void gfx_plot(int x, int y)
{
    a2addr a = gfx_text_row(y >> 1) + x;
    unsigned char b = a2mem[a];
    if (y & 1)
        b = (unsigned char)((b & 0x0F) | (color << 4));
    else
        b = (unsigned char)((b & 0xF0) | color);
    a2mem[a] = b;
    gfx_touch(a);
}

void gfx_hlin(int x1, int x2, int y)
{
    int x;
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    for (x = x1; x <= x2; x++)
        gfx_plot(x, y);
}

void gfx_vlin(int y1, int y2, int x)
{
    int y;
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    for (y = y1; y <= y2; y++)
        gfx_plot(x, y);
}

int gfx_scrn(int x, int y)
{
    unsigned char b = a2mem[gfx_text_row(y >> 1) + x];
    return (y & 1) ? (b >> 4) : (b & 0x0F);
}

void gfx_hplot(int x, int y)
{
    a2addr a = gfx_hires_row(hpage, y) + x / 7;
    int bit = x % 7;
    unsigned char b = a2mem[a];

    /* Plot only if this colour lights this column. A single green dot at an
     * even x therefore draws nothing at all -- that is the hardware, not a
     * bug, and it is why HGR art is full of two-pixel-wide strokes. */
    if (hcolor_lights(hcolor, x))
        b |= (unsigned char)(1 << bit);
    else
        b &= (unsigned char)~(1 << bit);

    /* Bit 7 belongs to the whole byte, which is why plotting one orange dot
     * can recolour the six pixels beside it. */
    if (hcolor & 4)
        b |= 0x80;
    else
        b &= 0x7F;

    a2mem[a] = b;
    last_x = x;
    last_y = y;
    gfx_touch(a);
}

void gfx_hplot_to(int x, int y)
{
    /* Bresenham from the last plotted point. */
    int x0 = last_x, y0 = last_y;
    int dx = x - x0, dy = y - y0;
    int sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1;
    int err, e2;

    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    err = dx - dy;

    for (;;) {
        gfx_hplot(x0, y0);
        if (x0 == x && y0 == y)
            break;
        e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
    last_x = x;
    last_y = y;
}

void gfx_last(int *x, int *y) { *x = last_x; *y = last_y; }

int gfx_hires_locate(a2addr a, int *y, int *col)
{
    a2addr page = gfx_hpage();
    unsigned int o, low;

    if (a < page || a >= page + 0x2000)
        return 0;
    o = (unsigned int)(a - page);
    low = (o & 0x7F) / 0x28;
    *col = (int)((o & 0x7F) % 0x28);
    if (low > 2 || *col >= 40)
        return 0;                      /* a hole in the layout, not a pixel */
    *y = (int)((o >> 10) | (((o & 0x3FF) >> 7) << 3) | (low << 6));
    return (*y < 192);
}

int gfx_lores_locate(a2addr a, int *row, int *col)
{
    unsigned int o, low;

    if (a < LORES_PAGE1 || a >= LORES_PAGE1 + 0x400)
        return 0;
    o = (unsigned int)(a - LORES_PAGE1);
    low = (o & 0x7F) / 0x28;
    *col = (int)((o & 0x7F) % 0x28);
    if (low > 2 || *col >= 40)
        return 0;
    *row = (int)((o >> 7) | (low << 3));
    return (*row < 24);
}
