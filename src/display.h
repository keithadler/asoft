/* display.h - showing the pages, per platform.
 *
 * Applesoft has no "draw the screen" statement: GR and HGR flip soft
 * switches and everything after that is just memory writes. So the display
 * follows page memory rather than being told what to paint, which also means
 * a program that POKEs $2000 -- or $400, the text page -- directly gets the
 * same result as one using HPLOT or PRINT. gfx.c calls disp_touch for every
 * byte it changes and for every switch it flips.
 *
 * Two implementations:
 *   display_dos.c   Real DOS hardware, and js-dos in a browser. BIOS text
 *                   mode 1 or 3 for the forty- and eighty-column text pages,
 *                   VGA mode 13h for the graphics pages, with the four text
 *                   lines of a mixed-mode screen drawn under the picture in
 *                   the Apple's own font. Every switch -- PR#0, PR#3, TEXT,
 *                   GR, HGR, HGR2, and the POKEs that flip them by hand --
 *                   comes through here and ends in a mode set and a repaint
 *                   from page memory. That is the whole trick: the text page
 *                   survives a trip through hi-res because it was never
 *                   anywhere but $400.
 *   display_term.c  ANSI half-blocks, so the native build can show a picture.
 */
#ifndef ASOFT_DISPLAY_H
#define ASOFT_DISPLAY_H

#include "a2mem.h"

void disp_init(void);
void disp_shutdown(void);

/* Called when the switches change, and for any page byte that changes.
 * changed == 0 means "everything". */
void disp_touch(a2addr changed);

/* Repaint whatever is pending. Cheap to call when nothing has changed. */
void disp_refresh(void);

/* Non-zero when the display is showing graphics. */
int  disp_graphics(void);

/* Non-zero when a front end without a page should swallow text rather than
 * print it: the DOS build says yes while a full-screen graphics mode is up
 * and its output is a stream rather than a page, because there is nowhere
 * for the text to go. The terminal build says no: there is only one stream
 * there, and hiding the prompt would just look broken. */
int  disp_suppress_text(void);

#endif
