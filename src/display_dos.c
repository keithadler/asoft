/* The DOS video layer: text modes and VGA mode 13h, painted from the pages.
 *
 * Text is a BIOS text mode -- 1 for the forty-column page, 3 for eighty --
 * with the cells written straight into video memory from page memory, so
 * the screen is exactly what the page holds: POKE 1024,193 puts an A in
 * the corner, GR then TEXT shows the picture as inverse letters, and a trip
 * through HGR and back finds the text where it was, because it never left
 * $400.
 *
 * Graphics is mode 13h: 320x200 in 256 colours. Hi-res is 280x192, so it
 * sits centred with a small border, and lo-res is 40x48, which scales to
 * exactly 320x192 at 8x4 per cell. Both use the palette from gfx.c, loaded
 * into the DAC when the mode is set. In mixed mode the bottom four text
 * lines are drawn under the picture from the page at $400, in the Apple's
 * own font and through the same artifact rules as hi-res -- which is what
 * the hardware did, since text was just another thing on the same field.
 *
 * Plotted pixels and printed characters are written to video memory as they
 * happen, the way an Apple program writing its own page would be seen
 * immediately. Anything that changes the display some other way -- a
 * switch flipping, a clear, a scroll -- repaints the whole thing.
 */
#include "display.h"
#include "video_dos.h"
#include "appletext.h"
#include "gfx.h"
#include "hires.h"
#include "screen.h"

#include <dos.h>
#include <conio.h>
#include <string.h>

#define VGA_W 320
#define VGA_H 200
#define HIRES_X0 20              /* (320 - 280) / 2 */
#define HIRES_Y0 4               /* (200 - 192) / 2 */
#define TEXT_ROWS 24
#define MIXED_ROW 20             /* first text row shown under a picture */

/* 320 * 200 is 64000, which overflows a signed 16-bit int, so the count has
 * to be built from unsigned operands rather than cast after the fact. */
#define VGA_BYTES ((unsigned)VGA_W * (unsigned)VGA_H)

/* Video memory: the VGA frame buffer at A000, text cells at B800. Taken
 * through MK_FP so a host build can point them at arrays and check what
 * would have been on the screen (tests/dossim.c). */
static unsigned char far *vga;
static unsigned char far *vtext;

static int gfx_active;           /* mode 13h is currently set */
static int shown_mode = GFX_TEXT;
static int text_cols;            /* 40 or 80 once a console asked; 0 = none */
static int cur_row = -1, cur_col = -1;

/* The eighty-column page. The card kept its page in auxiliary memory, which
 * the 64K image has not got, so it lives here instead. */
static unsigned char wide[TEXT_ROWS][80];

/* BIOS int 10h function 0: set a video mode. Clears the screen too. */
static void set_video_mode(int mode)
{
    union REGS r;
    r.x.ax = (unsigned short)mode;
    int86(0x10, &r, &r);
}

/* The lo-res and hi-res colours into DAC entries 0..21, for mode 13h. */
static void load_palette(void)
{
    int i;
    outp(0x3C8, 0);
    for (i = 0; i < PAL_SIZE; i++) {
        /* The DAC takes six bits per channel, not eight. */
        outp(0x3C9, gfx_palette[i][0] >> 2);
        outp(0x3C9, gfx_palette[i][1] >> 2);
        outp(0x3C9, gfx_palette[i][2] >> 2);
    }
}

/* --- text mode ---------------------------------------------------------- */

/* A page byte as a PC glyph and attribute. Normal text keeps its seven bits
 * -- lower case shows as lower case, which a IIe did -- and the other two
 * ranges fold to the sixty-four glyphs the character generator had. */
static unsigned char glyph_of(unsigned char b)
{
    unsigned char g = (b & 0x80) ? (unsigned char)(b & 0x7F)
                                 : (unsigned char)(b & 0x3F);
    return (g < 0x20) ? (unsigned char)(g + 0x40) : g;
}

/* The PC attribute for a page byte: normal, flashing (the blink bit, which
 * text modes 1 and 3 have on by default), or inverse. */
static unsigned char attr_of(unsigned char b)
{
    if (b & 0x80) return 0x07;          /* normal: light on dark */
    if (b & 0x40) return 0x87;          /* flash: the blink bit */
    return 0x70;                        /* inverse */
}

/* The byte on show at a cell: from the real page in forty columns, from
 * the wide buffer in eighty. */
static unsigned char page_byte(int row, int col)
{
    return (text_cols == 80) ? wide[row][col] : gfx_text_get(row, col);
}

/* One cell into text video memory: glyph byte, attribute byte. */
static void paint_text_cell(int row, int col)
{
    unsigned char b = page_byte(row, col);
    unsigned char far *p = vtext + ((unsigned)row * (unsigned)text_cols + (unsigned)col) * 2;
    p[0] = glyph_of(b);
    p[1] = attr_of(b);
}

/* The whole page, after a mode set or a scroll. */
static void paint_text_all(void)
{
    int row, col;
    for (row = 0; row < TEXT_ROWS; row++)
        for (col = 0; col < text_cols; col++)
            paint_text_cell(row, col);
}

/* BIOS int 10h function 2: put the hardware cursor where the console says,
 * or off the bottom of the screen to hide it. */
static void place_cursor(void)
{
    union REGS r;
    r.h.ah = 2;
    r.h.bh = 0;
    if (cur_row < 0 || cur_col < 0) {
        /* Off the bottom is the portable way to hide it. */
        r.h.dh = 25;
        r.h.dl = 0;
    } else {
        r.h.dh = (unsigned char)cur_row;
        r.h.dl = (unsigned char)cur_col;
    }
    int86(0x10, &r, &r);
}

/* Into the text mode for the current width and show the page in it. With
 * no console (text_cols still 0) just leave the screen in mode 3. */
static void enter_text(void)
{
    set_video_mode(text_cols == 80 ? 0x03 : (text_cols == 40 ? 0x01 : 0x03));
    gfx_active = 0;
    if (text_cols) {
        paint_text_all();
        place_cursor();
    }
}

/* --- graphics mode ------------------------------------------------------ */

/* Into mode 13h, cleared, with our palette. */
static void enter_graphics(void)
{
    if (gfx_active)
        return;
    set_video_mode(0x13);
    load_palette();
    /* Clear explicitly rather than trusting the mode set to have done it.
     * Anything left behind shows up in the BIOS default palette, as stripes
     * in colours this program never uses. */
    _fmemset(vga, 0, VGA_BYTES);
    gfx_active = 1;
}

/* How many scan lines of the picture are on show: 160 with text under it. */
static int hires_limit(void) { return gfx_mixed() ? MIXED_ROW * 8 : HIRES_H; }
static int lores_limit(void) { return gfx_mixed() ? MIXED_ROW * 2 : LORES_H; }

/* Columns x0..x1 of hi-res line y, if that line is on show. */
static void paint_hires_span(int y, int x0, int x1)
{
    unsigned char row[HIRES_W];
    unsigned char far *dst;
    int x;

    if (y < 0 || y >= hires_limit())
        return;
    if (x0 < 0) x0 = 0;
    if (x1 >= HIRES_W) x1 = HIRES_W - 1;

    /* Only the columns that changed, not the whole row: this is called once
     * per plotted pixel, so the difference is very visible. */
    hires_span(y, x0, x1, row);
    dst = vga + (unsigned)(y + HIRES_Y0) * VGA_W + HIRES_X0;
    for (x = x0; x <= x1; x++)
        dst[x] = row[x];
}

/* One lo-res block: 8x4 screen pixels, if that row is on show. */
static void paint_lores_cell(int x, int y)
{
    unsigned char c;
    unsigned char far *dst;
    int r;

    if (y < 0 || y >= lores_limit())
        return;
    c = (unsigned char)lores_pixel(x, y);
    dst = vga + (unsigned)(y * 4 + HIRES_Y0) * VGA_W + x * 8;
    for (r = 0; r < 4; r++) {
        _fmemset(dst, c, 8);
        dst += VGA_W;
    }
}

/* The text under a picture: columns x0..x1 of text row `row`, eight scan
 * lines each, in screen pixels. Forty columns is the page at $400 through
 * the colour rules, seven pixels a cell over the picture's 280; eighty is
 * the wide page, four pixels a cell across the whole 320, as the card's
 * half-width characters were. A cell's neighbours get a look in because
 * the fringing rules reach two pixels either side. */
static void paint_text_gfx(int row, int x0, int x1)
{
    unsigned char out[ATEXT_WIDE_W];
    unsigned char far *dst;
    int line, x;
    int is_wide = (text_cols == 80);
    int width = is_wide ? ATEXT_WIDE_W : ATEXT_W;

    if (!gfx_mixed() || row < MIXED_ROW || row >= TEXT_ROWS)
        return;
    if (x0 < 0) x0 = 0;
    if (x1 >= width) x1 = width - 1;
    for (line = 0; line < 8; line++) {
        int y = row * 8 + line;
        if (is_wide)
            appletext_wide_row(&wide[0][0], y, out);
        else
            appletext_page_row(y, out);
        dst = vga + (unsigned)(y + HIRES_Y0) * VGA_W + (is_wide ? 0 : HIRES_X0);
        for (x = x0; x <= x1; x++)
            dst[x] = out[x];
    }
}

/* Everything mode 13h should be showing, from the pages. */
static void paint_all(void)
{
    int y, x, row;

    if (!gfx_active)
        return;
    _fmemset(vga, 0, VGA_BYTES);
    if (gfx_mode() == GFX_HIRES) {
        for (y = 0; y < hires_limit(); y++)
            paint_hires_span(y, 0, HIRES_W - 1);
    } else if (gfx_mode() == GFX_LORES) {
        for (y = 0; y < lores_limit(); y++)
            for (x = 0; x < LORES_W; x++)
                paint_lores_cell(x, y);
    }
    if (gfx_mixed())
        for (row = MIXED_ROW; row < TEXT_ROWS; row++)
            paint_text_gfx(row, 0, ATEXT_WIDE_W - 1);
}

/* Whatever is up, drawn again from the page. */
static void repaint(void)
{
    if (gfx_active)
        paint_all();
    else if (text_cols)
        paint_text_all();
}

/* --- interface ---------------------------------------------------------- */

void disp_init(void)
{
    vga = (unsigned char far *)MK_FP(0xA000, 0);
    vtext = (unsigned char far *)MK_FP(0xB800, 0);
    gfx_active = 0;
    text_cols = 0;
    shown_mode = GFX_TEXT;
    cur_row = cur_col = -1;
    memset(wide, TEXT_BLANK, sizeof(wide));
}

void disp_shutdown(void)
{
    set_video_mode(0x03);
    gfx_active = 0;
}

/* Something in the pages or the switches changed. A switch change means a
 * mode set or a full repaint; a single byte means the cell, pixel span or
 * character it covers, and only if the display is showing that page. */
void disp_touch(a2addr changed)
{
    int mode = gfx_mode();
    int row, col, y;

    if (mode != shown_mode) {
        shown_mode = mode;
        if (mode == GFX_TEXT) {
            enter_text();
        } else {
            enter_graphics();
            paint_all();
        }
        return;
    }
    if (changed == 0) {
        repaint();
        return;
    }

    if (!gfx_active) {
        /* Forty columns is the page at $400; a byte there is a cell. In
         * eighty columns the page on show is the wide buffer, painted as it
         * is written, so nothing in the image can change the screen. */
        if (text_cols == 40 && gfx_lores_locate(changed, &row, &col))
            paint_text_cell(row, col);
        return;
    }

    /* A single byte: repaint just what it covers. Hi-res needs a pixel either
     * side too, because a neighbour decides whether a dot reads as white. */
    if (mode == GFX_HIRES && gfx_hires_locate(changed, &y, &col)) {
        paint_hires_span(y, col * 7 - 1, col * 7 + 7);
        return;
    }
    if (gfx_lores_locate(changed, &row, &col)) {
        if (row < MIXED_ROW || !gfx_mixed()) {
            if (mode == GFX_LORES) {
                paint_lores_cell(col, row * 2);
                paint_lores_cell(col, row * 2 + 1);
            }
        } else {
            paint_text_gfx(row, col * 7 - 2, col * 7 + 8);
        }
    }
}

void disp_refresh(void)
{
    /* Nothing is ever pending: every change paints as it happens. */
}

int disp_graphics(void)
{
    return gfx_mode() != GFX_TEXT;
}

int disp_suppress_text(void)
{
    /* For the stream fallback only: with hi-res up there is no page for the
     * text to land on, so it goes nowhere, as it did on the machine. */
    return gfx_active && gfx_mode() == GFX_HIRES;
}

/* --- the console's text calls ------------------------------------------- */

/* PR#0 or PR#3: pick the width, clear both pages, and if text is on show
 * switch the mode now. */
void vid_text_cols(int n)
{
    if (n != 40 && n != 80)
        return;
    text_cols = n;
    memset(wide, TEXT_BLANK, sizeof(wide));
    if (!gfx_active)
        enter_text();
    gfx_text_clear(0, TEXT_ROWS, 0, 40);
}

int vid_cols(void) { return text_cols; }

/* A character into the page on show. In forty columns that is the memory
 * image, and the change hook paints it; the wide buffer is painted here. */
void vid_text_put(int row, int col, unsigned char b)
{
    if (row < 0 || row >= TEXT_ROWS || col < 0 || col >= text_cols)
        return;
    if (text_cols == 80) {
        wide[row][col] = b;
        if (!gfx_active)
            paint_text_cell(row, col);
        else
            paint_text_gfx(row, col * 4, col * 4 + 3);
        return;
    }
    gfx_text_put(row, col, b);      /* the change hook paints it */
}

/* Keep a window inside the wide page. */
static void wide_clip(int *top, int *bottom, int *left, int *width)
{
    if (*top < 0) *top = 0;
    if (*bottom > TEXT_ROWS) *bottom = TEXT_ROWS;
    if (*left < 0) *left = 0;
    if (*left > 79) *left = 79;
    if (*left + *width > 80) *width = 80 - *left;
    if (*width < 0) *width = 0;
}

/* HOME: the window to blanks. */
void vid_text_clear(int top, int bottom, int left, int width)
{
    int row;
    if (text_cols == 80) {
        wide_clip(&top, &bottom, &left, &width);
        for (row = top; row < bottom; row++)
            memset(&wide[row][left], TEXT_BLANK, (size_t)width);
        repaint();
        return;
    }
    gfx_text_clear(top, bottom, left, width);
}

/* The window up one row, its bottom row blank: the text window scrolling. */
void vid_text_scroll(int top, int bottom, int left, int width)
{
    int row;
    if (text_cols == 80) {
        wide_clip(&top, &bottom, &left, &width);
        for (row = top; row + 1 < bottom; row++)
            memcpy(&wide[row][left], &wide[row + 1][left], (size_t)width);
        if (bottom > top)
            memset(&wide[bottom - 1][left], TEXT_BLANK, (size_t)width);
        repaint();
        return;
    }
    gfx_text_scroll(top, bottom, left, width);
}

/* Clear to end of line: blanks from col0 to col1 on one row. */
void vid_text_fill(int row, int col0, int col1)
{
    int col;
    if (row < 0 || row >= TEXT_ROWS)
        return;
    if (text_cols == 80) {
        if (col0 < 0) col0 = 0;
        if (col1 > 79) col1 = 79;
        for (col = col0; col <= col1; col++)
            wide[row][col] = TEXT_BLANK;
        if (!gfx_active)
            for (col = col0; col <= col1; col++)
                paint_text_cell(row, col);
        return;
    }
    gfx_text_fill(row, col0, col1);
}

/* Where the caret shows while a line is being typed; row -1 hides it. */
void vid_text_cursor(int row, int col)
{
    cur_row = row;
    cur_col = col;
    if (!gfx_active)
        place_cursor();
}
