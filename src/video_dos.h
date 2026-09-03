/* video_dos.h - the text side of the DOS video layer.
 *
 * display_dos.c owns the screen: which BIOS mode is up, and painting the
 * page memory into it. These are the calls the console makes to put text
 * on that screen. In forty columns the text page is the real one at $400,
 * so PEEK sees it and the four lines under a GR picture come from it; in
 * eighty columns it is a buffer here standing in for the auxiliary memory
 * the 80-column card kept its page in.
 */
#ifndef ASOFT_VIDEO_DOS_H
#define ASOFT_VIDEO_DOS_H

/* Choose the width: 40 is BIOS mode 1, 80 is mode 3. Clears the screen,
 * as PR#3 and PR#0 did. If graphics are up, the mode waits for TEXT. */
void vid_text_cols(int n);
int  vid_cols(void);

/* A byte of the page, in the machine's encoding (see gfx.h). */
void vid_text_put(int row, int col, unsigned char b);

/* Over the text window -- rows top..bottom-1, columns left..left+width-1:
 * clear it, or scroll it up by one with the bottom row blanked. */
void vid_text_clear(int top, int bottom, int left, int width);
void vid_text_scroll(int top, int bottom, int left, int width);
/* Blanks along one row, columns col0..col1: clear to end of line. */
void vid_text_fill(int row, int col0, int col1);

/* Where the caret shows; a negative row hides it. */
void vid_text_cursor(int row, int col);

#endif
