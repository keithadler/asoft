#include "gfx.h"
#include "bugs.h"

#include <string.h>

/* The four switches, kept as the bits the hardware kept. mode is what they
 * add up to, recomputed whenever one changes. */
static int sw_text = 1, sw_mixed, sw_page2, sw_hires;
static int mode = GFX_TEXT;
static int last_x, last_y;

/* The ROM's byte for each HCOLOR: which of seven columns it lights, and
 * bit 7 for the palette. */
static const unsigned char hcolor_byte[8] = {
    0x00, 0x2A, 0x55, 0x7F, 0x80, 0xAA, 0xD5, 0xFF
};

/* HCOLOR does not pick a colour so much as a bit pattern. The ROM keeps a
 * byte per colour and plots the bit that lands under x, which is why green
 * only lights odd columns and violet only even ones: a single dot of either
 * is invisible half the time, and that is the hardware being honest rather
 * than a bug. Bit 7 selects the palette pair (green/violet or orange/blue).
 *
 *   0 black1 $00   1 green  $2A   2 violet $55   3 white1 $7F
 *   4 black2 $80   5 orange $AA   6 blue   $D5   7 white2 $FF
 */
/* Does the colour byte at $E4 light column x? The ROM kept the pattern
 * and rotated it by one bit for every odd byte, because seven pixels to a
 * byte puts the parity of a column out of step with the parity of its bit
 * on every other byte: that is how 0x2A, bits 1, 3 and 5, lights the odd
 * columns of every byte in the row and not just the first. Reading $E4
 * rather than a private copy is what lets POKE 228,n pick a colour too. */
static int hcolor_lights(int x)
{
    unsigned char pattern = a2mem[ZP_HCOLOR];
    int bit = x % 7;
    if ((x / 7) & 1) {
        bit = (bit + 1) % 7;
        /* The rotated byte is what the ROM kept at $1C for this column. */
        a2mem[ZP_HCOLOR1] = (unsigned char)((pattern & 0x80) |
                            (((pattern & 0x7F) >> 1) | ((pattern & 1) << 6)));
    } else {
        a2mem[ZP_HCOLOR1] = pattern;
    }
    return (pattern >> bit) & 1;
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

/* What the four switches add up to. Text wins over everything; otherwise
 * the hi-res switch picks the page kind. */
static void settle(void)
{
    mode = sw_text ? GFX_TEXT : (sw_hires ? GFX_HIRES : GFX_LORES);
}

void gfx_reset(void)
{
    sw_text = 1;
    sw_mixed = 0;
    sw_page2 = 0;
    sw_hires = 0;
    settle();
    a2mem[ZP_COLOR] = 0;
    a2mem[ZP_HCOLOR] = hcolor_byte[3];
    a2mem[ZP_HPAG] = HIRES_PAGE1 >> 8;
    last_x = last_y = 0;
}

int gfx_mode(void) { return mode; }
int gfx_mixed(void) { return sw_mixed; }
int gfx_get_color(void) { return a2mem[ZP_COLOR] & 15; }

int gfx_get_hcolor(void)
{
    int i;
    for (i = 0; i < 8; i++)
        if (hcolor_byte[i] == a2mem[ZP_HCOLOR])
            return i;
    return -1;                          /* something POKEd, not an HCOLOR */
}

/* HPLOT draws on the page whose high byte is at $E6: $20 or $40 after HGR
 * or HGR2, or whatever POKE 230 put there. */
a2addr gfx_hpage(void) { return (a2addr)a2mem[ZP_HPAG] << 8; }
a2addr gfx_disp_hpage(void) { return sw_page2 ? HIRES_PAGE2 : HIRES_PAGE1; }

void gfx_notify(a2addr changed) { gfx_touch(changed); }

/* Is this one of the video switches at $C050-$C057? PEEK and POKE ask
 * before touching memory, because these are not memory. */
int gfx_is_softswitch(a2addr a)
{
    return a >= SW_TEXTOFF && a <= SW_HIRES;
}

/* Flip one switch, as any access to its address did on the machine. */
void gfx_softswitch(a2addr a)
{
    int was_mode = mode, was_mixed = sw_mixed, was_page = sw_page2;

    switch (a) {
    case SW_TEXTOFF: sw_text = 0;  break;
    case SW_TEXTON:  sw_text = 1;  break;
    case SW_MIXOFF:  sw_mixed = 0; break;
    case SW_MIXON:   sw_mixed = 1; break;
    case SW_PAGE1:   sw_page2 = 0; break;
    case SW_PAGE2:   sw_page2 = 1; break;
    case SW_LORES:   sw_hires = 0; break;
    case SW_HIRES:   sw_hires = 1; break;
    default: return;
    }
    settle();
    /* Anything that changes what is on the display is a full repaint; the
     * display cannot know which of its pixels a switch just moved. */
    if (mode != was_mode || sw_mixed != was_mixed || sw_page2 != was_page)
        gfx_touch(0);
}

int gfx_hscrn(int x, int y)
{
    a2addr a;
    if (x < 0 || x >= 280 || y < 0 || y >= 192)
        return 0;
    a = gfx_hires_row(gfx_hpage(), y) + x / 7;
    return (a2mem[a] >> (x % 7)) & 1;
}

void gfx_hxor(int x, int y)
{
    a2addr a;
    if (x < 0 || x >= 280 || y < 0 || y >= 192)
        return;
    a = gfx_hires_row(gfx_hpage(), y) + x / 7;
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

/* --- the text page -------------------------------------------------------
 * Same memory as lo-res, which is why GR then TEXT shows the picture as
 * inverse letters and why PLOT over a line of text corrupts it. Both are
 * the machine, not us. */

/* Store one character byte in the page and tell the display. */
void gfx_text_put(int row, int col, unsigned char b)
{
    a2addr a;
    if (row < 0 || row >= 24 || col < 0 || col >= 40)
        return;
    a = gfx_text_row(row) + col;
    a2mem[a] = b;
    gfx_touch(a);
}

unsigned char gfx_text_get(int row, int col)
{
    if (row < 0 || row >= 24 || col < 0 || col >= 40)
        return TEXT_BLANK;
    return a2mem[gfx_text_row(row) + col];
}

/* HOME over the text window: rows top..23 to blanks. */
/* Clip a window to the page. */
static void window_clip(int *top, int *bottom, int *left, int *width)
{
    if (*top < 0) *top = 0;
    if (*bottom > 24) *bottom = 24;
    if (*left < 0) *left = 0;
    if (*left > 39) *left = 39;
    if (*left + *width > 40) *width = 40 - *left;
    if (*width < 0) *width = 0;
}

void gfx_text_clear(int top, int bottom, int left, int width)
{
    int row;
    window_clip(&top, &bottom, &left, &width);
    /* Only the window's cells: the eight-byte holes between rows are not
     * on the screen and programs used them for their own storage, and a
     * status line outside the window is exactly what POKE 34 protects. */
    for (row = top; row < bottom; row++)
        memset(&a2mem[gfx_text_row(row) + left], TEXT_BLANK, (size_t)width);
    gfx_touch(0);
}

/* Scroll the text window: rows top..23 move up one, and the bottom row is
 * blanked. The picture scrolls with it when the window covers it, because
 * it is the same memory. */
void gfx_text_scroll(int top, int bottom, int left, int width)
{
    int row;
    window_clip(&top, &bottom, &left, &width);
    for (row = top; row + 1 < bottom; row++)
        memcpy(&a2mem[gfx_text_row(row) + left],
               &a2mem[gfx_text_row(row + 1) + left], (size_t)width);
    if (bottom > top)
        memset(&a2mem[gfx_text_row(bottom - 1) + left], TEXT_BLANK, (size_t)width);
    gfx_touch(0);
}

void gfx_text_fill(int row, int col0, int col1)
{
    if (row < 0 || row >= 24)
        return;
    if (col0 < 0) col0 = 0;
    if (col1 > 39) col1 = 39;
    if (col1 < col0)
        return;
    memset(&a2mem[gfx_text_row(row) + col0], TEXT_BLANK, (size_t)(col1 - col0 + 1));
    gfx_touch(0);
}

static void clear_hires(a2addr page)
{
    int y;
    for (y = 0; y < 192; y++)
        memset(&a2mem[gfx_hires_row(page, y)], 0, 40);
}

/* The statements are the switches the ROM flipped for each, plus a clear.
 * GR and HGR are mixed -- four lines of text stay under the picture -- and
 * HGR2 is the whole screen, on page 2. TEXT flips one switch and nothing
 * else, so a later GR finds mixed mode still set. */
void gfx_gr(void)
{
    sw_text = 0; sw_mixed = 1; sw_page2 = 0; sw_hires = 0;
    settle();
    a2mem[ZP_COLOR] = 0;
    clear_lores();
    gfx_touch(0);
}

/* CALL -1998 ($F832): every lo-res block black, text lines included. */
void gfx_clrscr(void)
{
    int row;
    for (row = 0; row < 24; row++)
        memset(&a2mem[gfx_text_row(row)], 0, 40);
    gfx_touch(0);
}

/* CALL -1994 ($F836): the top forty rows, which GR itself does. */
void gfx_clrtop(void)
{
    clear_lores();
    gfx_touch(0);
}

void gfx_text(void)
{
    sw_text = 1;
    settle();
    gfx_touch(0);
}

void gfx_hgr(void)
{
    sw_text = 0; sw_mixed = 1; sw_page2 = 0; sw_hires = 1;
    settle();
    a2mem[ZP_HPAG] = HIRES_PAGE1 >> 8;
    clear_hires(HIRES_PAGE1);
    last_x = last_y = 0;
    gfx_touch(0);
}

void gfx_hgr2(void)
{
    sw_text = 0; sw_mixed = 0; sw_page2 = 1; sw_hires = 1;
    settle();
    a2mem[ZP_HPAG] = HIRES_PAGE2 >> 8;
    clear_hires(HIRES_PAGE2);
    last_x = last_y = 0;
    gfx_touch(0);
}

/* CALL 62450 ($F3F2): the page HPLOT draws on, to black. */
void gfx_hclr(void)
{
    clear_hires(gfx_hpage());
    gfx_touch(0);
}

/* CALL 62454 ($F3F6): the same page filled with the colour byte at $1C,
 * which is the colour of the last HPLOT, not of the last HCOLOR= -- the
 * ROM's BKGND read the byte its plotter had aligned, and a program that
 * set HCOLOR and called it without plotting got the previous colour. The
 * genuine ROM says so; see tools/diff/corpus/zp.bas. */
void gfx_bkgnd(void)
{
    int y;
    for (y = 0; y < 192; y++)
        memset(&a2mem[gfx_hires_row(gfx_hpage(), y)], a2mem[ZP_HCOLOR1], 40);
    gfx_touch(0);
}

/* The ROM stores the colour in both nibbles, so a POKE 48 can give the
 * odd and even rows different colours, which PLOT then honours. */
void gfx_color(int c)  { a2mem[ZP_COLOR] = (unsigned char)((c & 15) * 17); }
void gfx_hcolor(int c) { a2mem[ZP_HCOLOR] = hcolor_byte[c & 7]; }

void gfx_plot(int x, int y)
{
    a2addr a = gfx_text_row(y >> 1) + x;
    unsigned char b = a2mem[a];
    if (y & 1)
        b = (unsigned char)((b & 0x0F) | (a2mem[ZP_COLOR] & 0xF0));
    else
        b = (unsigned char)((b & 0xF0) | (a2mem[ZP_COLOR] & 0x0F));
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
    a2addr a = gfx_hires_row(gfx_hpage(), y) + x / 7;
    int bit = x % 7;
    unsigned char b = a2mem[a];

    /* Plot only if this colour lights this column. A single green dot at an
     * even x therefore draws nothing at all -- that is the hardware, not a
     * bug, and it is why HGR art is full of two-pixel-wide strokes. */
    if (hcolor_lights(x))
        b |= (unsigned char)(1 << bit);
    else
        b &= (unsigned char)~(1 << bit);

    /* Bit 7 belongs to the whole byte, which is why plotting one orange dot
     * can recolour the six pixels beside it. */
    if (a2mem[ZP_HCOLOR] & 0x80)
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
    /* The page on the display, not the one being drawn on: a program that
     * draws on page 1 while showing page 2 is showing you nothing yet. */
    a2addr page = gfx_disp_hpage();
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
