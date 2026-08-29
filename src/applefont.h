/* applefont.h - the 5x7 character set, in the Apple's 7x8 cell.
 *
 * Text on an Apple II was not a separate kind of thing from graphics: the
 * character generator put 5x7 dots into a 7x8 cell on the same 280x192 field
 * at the same dot clock, which is why text on a colour monitor picked up the
 * same fringing hi-res did. Rendering text into a bitmap and running it
 * through the artifact rules is therefore not a trick -- it is what the
 * hardware was doing.
 *
 * The shapes are authored, not lifted: the real character ROM is still
 * Apple's. Each row is one scan line, bit 4 leftmost, and the eighth is blank.
 */
#ifndef ASOFT_APPLEFONT_H
#define ASOFT_APPLEFONT_H

#define APPLE_FONT_FIRST 0x20
#define APPLE_FONT_CHARS 64
#define APPLE_CELL_W 7
#define APPLE_CELL_H 8

extern const unsigned char apple_font[APPLE_FONT_CHARS][8];

#endif
