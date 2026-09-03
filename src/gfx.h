/* gfx.h - lo-res and hi-res plotting into the memory image.
 *
 * The reference build parses the graphics statements and does nothing with
 * them. We plot for real, into the page memory the Apple actually used, so
 * PEEK sees what PLOT drew and a program that POKEs the screen directly
 * interoperates with one that uses HPLOT.
 *
 * Neither page is laid out the way you would expect, because both were wired
 * to make the video hardware's job easy rather than the programmer's:
 *
 *   lo-res / text, 40x48 (24 text rows, two pixels stacked per row):
 *       base = $400 + (row & 7) * $80 + (row >> 3) * $28
 *       even y in the low nibble, odd y in the high nibble
 *
 *   hi-res, 280x192:
 *       base = $2000 + (y & 7) * $400 + ((y >> 3) & 7) * $80 + (y >> 6) * $28
 *       seven pixels per byte, bit 7 selecting the colour pair
 *
 * So consecutive rows are nowhere near each other, and row 64 sits $28 bytes
 * after row 0. That is not a quirk worth hiding: programs of the period knew
 * it and relied on it.
 */
#ifndef ASOFT_GFX_H
#define ASOFT_GFX_H

#include "a2mem.h"

#define GFX_TEXT  0
#define GFX_LORES 1
#define GFX_HIRES 2

#define LORES_PAGE1 0x0400
#define HIRES_PAGE1 0x2000
#define HIRES_PAGE2 0x4000

/* The video soft switches. Reading or writing any of them flips it; that
 * is how the ROM's TEXT, GR, HGR and HGR2 work, and how a program flips
 * them itself with POKE -16302,0 (full screen) or POKE -16299,0 (page 2
 * while still drawing on page 1). What is on the display is entirely a
 * function of these four bits. */
#define SW_TEXTOFF  0xC050
#define SW_TEXTON   0xC051
#define SW_MIXOFF   0xC052
#define SW_MIXON    0xC053
#define SW_PAGE1    0xC054
#define SW_PAGE2    0xC055
#define SW_LORES    0xC056
#define SW_HIRES    0xC057
int  gfx_is_softswitch(a2addr a);
void gfx_softswitch(a2addr a);        /* touch one, as PEEK or POKE does */

/* The display registers here so it can follow plotting without rescanning
 * the page. The argument is the byte that changed, or 0 for "everything". */
typedef void (*gfx_hook)(a2addr changed);
void   gfx_on_change(gfx_hook h);

a2addr gfx_hpage(void);       /* base of the page HPLOT is drawing on */
a2addr gfx_disp_hpage(void);  /* base of the hi-res page the display shows */
int    gfx_mixed(void);       /* four lines of text under the picture? */

/* Tell the display a byte changed by some route other than plotting: POKE
 * into the text page or a hi-res page shows up the same way HPLOT does. */
void   gfx_notify(a2addr changed);

/* Undo the interleave: which scan line and which byte of it does this address
 * hold? Returns 0 for an address off the page, or in one of the eight-byte
 * holes the layout leaves at the end of every group of rows. A display uses
 * these to repaint just what changed. */
int gfx_hires_locate(a2addr a, int *y, int *col);
int gfx_lores_locate(a2addr a, int *row, int *col);

void   gfx_reset(void);
/* What the switches add up to: text, lo-res or hi-res. */
int    gfx_mode(void);

/* Address of the byte holding text/lo-res row 0..23. */
a2addr gfx_text_row(int row);

/* The text page, in the machine's own encoding: bit 7 set is normal text,
 * $00-$3F inverse, $40-$7F flashing. A blank is $A0, which is why a fresh
 * screen is full of $A0 and not zero -- zero is an inverse "@". Row 23 is
 * the bottom; scrolling and clearing take the window top, as the ROM did,
 * so GR's four-line window scrolls without disturbing the picture. */
#define TEXT_BLANK 0xA0
void gfx_text_put(int row, int col, unsigned char b);
unsigned char gfx_text_get(int row, int col);
/* Over a window: rows top..bottom-1, columns left..left+width-1. */
void gfx_text_clear(int top, int bottom, int left, int width);
void gfx_text_scroll(int top, int bottom, int left, int width);
void gfx_text_fill(int row, int col0, int col1);   /* blanks along one row */
/* Address of the byte holding hi-res y 0..191 on the given page base. */
a2addr gfx_hires_row(a2addr page, int y);

void gfx_gr(void);            /* lo-res, clear, four text lines kept */
void gfx_hgr(void);           /* hi-res page 1, clear */
void gfx_hgr2(void);          /* hi-res page 2, clear */
void gfx_text(void);

/* COLOR= and HCOLOR= live in zero page as the ROM kept them -- $30 holds
 * the lo-res colour in both nibbles, $E4 the hi-res bit pattern, $E6 the
 * page HPLOT draws on -- so a program that POKEs them gets what it asked
 * for, and a PEEK reads back what it set. */
void gfx_color(int c);        /* COLOR= 0..15 */
void gfx_hcolor(int c);       /* HCOLOR= 0..7 */

/* The Monitor entry points programs CALLed: CALL -1998 and -1994 clear
 * the lo-res screen, all of it or above the text window; CALL 62450
 * clears the hi-res page to black and CALL 62454 to the current colour. */
void gfx_clrscr(void);
void gfx_clrtop(void);
void gfx_hclr(void);
void gfx_bkgnd(void);

/* Palette indices the rasterizer and the display agree on: 0..15 are the
 * lo-res colours, 16..21 the six hi-res ones. */
#define PAL_LORES 0
#define PAL_HIRES 16
#define PAL_SIZE  22
extern const unsigned char gfx_palette[PAL_SIZE][3];   /* r, g, b */
int  gfx_get_color(void);
int  gfx_get_hcolor(void);

void gfx_plot(int x, int y);              /* lo-res, x 0..39 y 0..47 */
void gfx_hlin(int x1, int x2, int y);
void gfx_vlin(int y1, int y2, int x);
int  gfx_scrn(int x, int y);              /* SCRN(x,y) */

int  gfx_hscrn(int x, int y);             /* is this hi-res pixel lit? */
void gfx_hxor(int x, int y);              /* toggle it, which is what XDRAW does */

void gfx_hplot(int x, int y);             /* hi-res, x 0..279 y 0..191 */
void gfx_hplot_to(int x, int y);          /* line from the last point */
void gfx_last(int *x, int *y);

#endif
