/* appletext.h - the 40x24 text screen as the display actually showed it.
 *
 * A PC text cell cannot be an Apple character: it is a CP437 glyph in a PC
 * font, at the wrong size, with no way to produce colour fringing. So the
 * text screen is rendered the way the hardware made it -- 5x7 dots in a 7x8
 * cell on the same 280x192 field as hi-res -- and run through the same
 * artifact rules. That is why white text on an Apple came out fringed green
 * and violet on a colour monitor, and it is what this reproduces.
 */
#ifndef ASOFT_APPLETEXT_H
#define ASOFT_APPLETEXT_H

#define ATEXT_COLS 40
#define ATEXT_ROWS 24
#define ATEXT_W 280
#define ATEXT_H 192

/* Render the whole screen. out holds ATEXT_W * ATEXT_H palette indices. */
void appletext_render(const char *cells, unsigned char *out);

/* One scan line, for a display that repaints in strips. */
void appletext_row(const char *cells, int y, unsigned char *out);

#endif
