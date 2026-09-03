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

/* One scan line of the real text page at $400, in the machine's encoding:
 * bit 7 clear is inverse, and inverse inverts the whole 7x8 cell, gap
 * columns included, the way the character generator did. Flashing text is
 * drawn inverse and lower case as the capitals, since the font has sixty-
 * four glyphs. This is what the four text lines under GR and HGR are
 * drawn from. */
void appletext_page_row(int y, unsigned char *out);

/* One scan line of an eighty-column page -- 24 rows of 80 bytes in the same
 * encoding -- across the full 320 pixels of the display, four to a cell.
 * The 80-column card's characters were half the width of the 40-column
 * ones, and so are these: three of the glyph's five columns and a gap, in
 * white, without the colour rules, which at that pitch would only smear.
 * out holds 320 palette indices. */
#define ATEXT_WIDE_W 320
void appletext_wide_row(const unsigned char *page, int y, unsigned char *out);

#endif
