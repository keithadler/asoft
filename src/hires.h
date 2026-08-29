/* hires.h - turn Apple page memory into pixels.
 *
 * Both graphics pages are stored the way the hardware wanted them, not the
 * way anything wants to draw them, so this is where that gets undone. It is
 * portable: the DOS build blits the result to VGA, the console build draws it
 * with terminal half-blocks, and neither has to understand the layout.
 *
 * Hi-res colour is a property of position, not of the bit. A lit pixel is
 * white if either neighbour is lit; otherwise it takes its colour from the
 * column's parity and bit 7 of the byte it lives in -- violet or blue on even
 * columns, green or orange on odd ones. That is genuinely how the display
 * worked: the pixel clock ran at the colour subcarrier frequency, so where a
 * dot sat decided what colour it came out.
 */
#ifndef ASOFT_HIRES_H
#define ASOFT_HIRES_H

#include "gfx.h"

#define HIRES_W 280
#define HIRES_H 192
#define LORES_W 40
#define LORES_H 48

/* The artifact rules over one row of 40 bytes, seven pixels each. Exposed
 * because the text screen goes through them too. */
void artifact_span(const unsigned char *row, int x0, int x1, unsigned char *out);

/* Palette index (see gfx_palette) for one hi-res pixel. */
int hires_pixel(int x, int y);

/* One row of hi-res pixels as palette indices. out needs HIRES_W bytes. */
void hires_row(int y, unsigned char *out);

/* Just the columns x0..x1 of a row, written into out at their own indices.
 * out still needs HIRES_W bytes, but only that slice is computed -- which
 * matters on a real DOS machine, where recomputing all 280 pixels for every
 * plotted dot is the difference between a picture appearing and a picture
 * crawling. A couple of columns either side are examined but not written,
 * since a pixel's colour depends on its neighbours. */
void hires_span(int y, int x0, int x1, unsigned char *out);

/* Palette index for one lo-res cell. */
int lores_pixel(int x, int y);

#endif
