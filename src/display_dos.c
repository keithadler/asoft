/* VGA mode 13h display for the DOS build.
 *
 * 320x200 in 256 colours is a comfortable fit: hi-res is 280x192, so it sits
 * centred with a small border, and lo-res is 40x48, which scales to exactly
 * 320x192 at 8x4 per cell. Both use the palette from gfx.c, loaded into the
 * DAC at startup.
 *
 * Plotted pixels are written straight to video memory as they happen, the way
 * an Apple program writing its own page would be seen immediately. Anything
 * that changes the page some other way -- POKE, or a screen clear -- asks for
 * a full repaint instead.
 */
#include "display.h"
#include "gfx.h"
#include "hires.h"

#include <dos.h>
#include <conio.h>
#include <string.h>

#define VGA_W 320
#define VGA_H 200
#define HIRES_X0 20              /* (320 - 280) / 2 */
#define HIRES_Y0 4               /* (200 - 192) / 2 */

static unsigned char far *vga = (unsigned char far *)0xA0000000L;
static int active;               /* mode 13h is currently set */
static int dirty;                /* a full repaint is pending */
static int shown_mode = GFX_TEXT;

static void set_video_mode(int mode)
{
    union REGS r;
    r.x.ax = (unsigned short)mode;
    int86(0x10, &r, &r);
}

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

/* 320 * 200 is 64000, which overflows a signed 16-bit int, so the count has
 * to be built from unsigned operands rather than cast after the fact. */
#define VGA_BYTES ((unsigned)VGA_W * (unsigned)VGA_H)

static void enter_graphics(void)
{
    if (active)
        return;
    set_video_mode(0x13);
    load_palette();
    /* Clear explicitly rather than trusting the mode set to have done it.
     * Anything left behind shows up in the BIOS default palette, as stripes
     * in colours this program never uses. */
    _fmemset(vga, 0, VGA_BYTES);
    active = 1;
}

static void leave_graphics(void)
{
    if (!active)
        return;
    set_video_mode(0x03);
    active = 0;
}

/* --- address to screen position ------------------------------------------
 * The interleave inversion lives in gfx.c so it can be tested on a host
 * compiler; see tests/test_gfx.c. */

/* --- painting ----------------------------------------------------------- */

static void paint_hires_span(int y, int x0, int x1)
{
    unsigned char row[HIRES_W];
    unsigned char far *dst;
    int x;

    if (y < 0 || y >= HIRES_H)
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

static void paint_lores_cell(int x, int y)
{
    unsigned char c = (unsigned char)lores_pixel(x, y);
    unsigned char far *dst = vga + (unsigned)(y * 4 + HIRES_Y0) * VGA_W + x * 8;
    int r;
    for (r = 0; r < 4; r++) {
        _fmemset(dst, c, 8);
        dst += VGA_W;
    }
}

static void paint_all(void)
{
    int y, x;

    if (gfx_mode() == GFX_HIRES) {
        _fmemset(vga, 0, VGA_BYTES);
        for (y = 0; y < HIRES_H; y++)
            paint_hires_span(y, 0, HIRES_W - 1);
    } else if (gfx_mode() == GFX_LORES) {
        _fmemset(vga, 0, VGA_BYTES);
        for (y = 0; y < LORES_H; y++)
            for (x = 0; x < LORES_W; x++)
                paint_lores_cell(x, y);
    }
}

/* --- interface ---------------------------------------------------------- */

void disp_init(void)
{
    active = 0;
    dirty = 0;
    shown_mode = GFX_TEXT;
}

void disp_shutdown(void)
{
    leave_graphics();
}

void disp_touch(a2addr changed)
{
    int mode = gfx_mode();

    if (mode != shown_mode) {
        shown_mode = mode;
        if (mode == GFX_TEXT)
            leave_graphics();
        else {
            enter_graphics();
            dirty = 1;
        }
        return;
    }
    if (mode == GFX_TEXT || !active)
        return;

    if (changed == 0) {
        dirty = 1;
        return;
    }

    /* A single byte: repaint just what it covers. Hi-res needs a pixel either
     * side too, because a neighbour decides whether a dot reads as white. */
    if (mode == GFX_HIRES) {
        int y, col;
        if (gfx_hires_locate(changed, &y, &col))
            paint_hires_span(y, col * 7 - 1, col * 7 + 7);
        else
            dirty = 1;
    } else {
        int row, col;
        if (gfx_lores_locate(changed, &row, &col)) {
            paint_lores_cell(col, row * 2);
            paint_lores_cell(col, row * 2 + 1);
        } else {
            dirty = 1;
        }
    }
}

void disp_refresh(void)
{
    if (!dirty)
        return;
    dirty = 0;
    if (gfx_mode() == GFX_TEXT)
        leave_graphics();
    else {
        enter_graphics();
        paint_all();
    }
}

int disp_graphics(void)
{
    return gfx_mode() != GFX_TEXT;
}

int disp_suppress_text(void)
{
    /* Lo-res keeps the bottom four text lines on an Apple; we have not got a
     * font in mode 13h to honour that, so only hi-res hides the text. */
    return active && gfx_mode() == GFX_HIRES;
}
