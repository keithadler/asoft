/* display.h - showing the graphics pages, per platform.
 *
 * Applesoft has no "draw the screen" statement: GR and HGR switch the display
 * over and everything after that is just memory writes. So the display
 * follows page memory rather than being told what to paint, which also means
 * a program that POKEs $2000 directly gets the same result as one using
 * HPLOT. gfx.c calls disp_touch for every byte it changes.
 *
 * Two implementations:
 *   display_dos.c   VGA mode 13h. Real DOS hardware, and js-dos in a browser.
 *   display_term.c  ANSI half-blocks, so the native build can show a picture.
 *
 * Text while graphics are up follows the Apple: in hi-res nothing is printed
 * to the screen at all, so the prompt does not scribble over the picture and
 * you type TEXT blind to get back. That is not an oversight; it is what the
 * machine did, because the text and graphics pages were different memory.
 */
#ifndef ASOFT_DISPLAY_H
#define ASOFT_DISPLAY_H

#include "a2mem.h"

void disp_init(void);
void disp_shutdown(void);

/* Called when GR/HGR/HGR2/TEXT switch modes, and for any page byte that
 * changes. changed == 0 means "everything". */
void disp_touch(a2addr changed);

/* Repaint whatever is pending. Cheap to call when nothing has changed. */
void disp_refresh(void);

/* Non-zero when the display is showing graphics, so the front end knows to
 * hold back text the way the Apple did. */
int  disp_graphics(void);

/* Non-zero when the front end should swallow text rather than print it. The
 * DOS build says yes while hi-res is up, because that is what an Apple did --
 * the prompt went to the text page, which was not the page on screen. The
 * terminal build says no: there is only one stream there, and hiding the
 * prompt would just look broken. */
int  disp_suppress_text(void);

#endif
