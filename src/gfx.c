#include "gfx.h"
#include "bugs.h"

#include <string.h>

static int mode = GFX_TEXT;
static int color;             /* lo-res colour 0..15 */
static int hcolor;            /* hi-res colour 0..7 */
static a2addr hpage = HIRES_PAGE1;
static int last_x, last_y;

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

void gfx_gr(void)   { mode = GFX_LORES; color = 0; clear_lores(); }
void gfx_text(void) { mode = GFX_TEXT; }

void gfx_hgr(void)
{
    mode = GFX_HIRES;
    hpage = HIRES_PAGE1;
    clear_hires(hpage);
    last_x = last_y = 0;
}

void gfx_hgr2(void)
{
    mode = GFX_HIRES;
    hpage = HIRES_PAGE2;
    clear_hires(hpage);
    last_x = last_y = 0;
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

    /* Colours 0..3 use the low palette, 4..7 set bit 7. Odd colours light
     * the pixel; even ones clear it, which is how HCOLOR=0 erases. */
    if (hcolor & 3)
        b |= (unsigned char)(1 << bit);
    else
        b &= (unsigned char)~(1 << bit);

    if (hcolor & 4)
        b |= 0x80;
    else
        b &= 0x7F;

    a2mem[a] = b;
    last_x = x;
    last_y = y;
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
