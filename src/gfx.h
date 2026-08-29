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

void   gfx_reset(void);
int    gfx_mode(void);

/* Address of the byte holding text/lo-res row 0..23. */
a2addr gfx_text_row(int row);
/* Address of the byte holding hi-res y 0..191 on the given page base. */
a2addr gfx_hires_row(a2addr page, int y);

void gfx_gr(void);            /* lo-res, clear, four text lines kept */
void gfx_hgr(void);           /* hi-res page 1, clear */
void gfx_hgr2(void);          /* hi-res page 2, clear */
void gfx_text(void);

void gfx_color(int c);        /* COLOR= 0..15 */
void gfx_hcolor(int c);       /* HCOLOR= 0..7 */
int  gfx_get_color(void);
int  gfx_get_hcolor(void);

void gfx_plot(int x, int y);              /* lo-res, x 0..39 y 0..47 */
void gfx_hlin(int x1, int x2, int y);
void gfx_vlin(int y1, int y2, int x);
int  gfx_scrn(int x, int y);              /* SCRN(x,y) */

void gfx_hplot(int x, int y);             /* hi-res, x 0..279 y 0..191 */
void gfx_hplot_to(int x, int y);          /* line from the last point */
void gfx_last(int *x, int *y);

#endif
