/* tui.h - a character screen, one buffer, two backends.
 *
 * Turbo Vision only exists as a Borland DOS library, so the IDE is drawn
 * against this instead: a cell buffer with an attribute per character, which
 * is exactly what text-mode video memory already is. The DOS backend writes
 * it straight to 0xB800; the terminal backend turns it into ANSI. That is the
 * same split display_dos.c and display_term.c use for graphics.
 *
 * Attributes are the CGA byte -- low nibble foreground, high nibble
 * background -- because that is what the hardware wants and inventing a
 * different scheme would only mean converting twice.
 */
#ifndef ASOFT_TUI_H
#define ASOFT_TUI_H

#define TUI_W 80
#define TUI_H 43

/* Colours, in the order the hardware numbered them. */
#define C_BLACK 0
#define C_BLUE 1
#define C_GREEN 2
#define C_CYAN 3
#define C_RED 4
#define C_MAGENTA 5
#define C_BROWN 6
#define C_LTGRAY 7
#define C_DKGRAY 8
#define C_LTBLUE 9
#define C_LTGREEN 10
#define C_LTCYAN 11
#define C_LTRED 12
#define C_LTMAGENTA 13
#define C_YELLOW 14
#define C_WHITE 15

#define ATTR(fg, bg) ((unsigned char)(((bg) << 4) | (fg)))

/* Glyphs the two backends spell differently: CP437 bytes on DOS, Unicode in a
 * terminal. Kept in the control range so they cannot collide with text. */
#define G_TL     1        /* box corners, double-line */
#define G_TR     2
#define G_BL     3
#define G_BR     4
#define G_HORZ   5
#define G_VERT   6
#define G_SHADE  7        /* the desktop's stipple */
#define G_RULE   8        /* single-line horizontal, for rules inside panes */

void tui_init(void);
void tui_shutdown(void);

void tui_clear(unsigned char attr);
void tui_put(int x, int y, int ch, unsigned char attr);
void tui_puts(int x, int y, const char *s, unsigned char attr);
void tui_fill(int x, int y, int w, int h, int ch, unsigned char attr);

/* A framed window with a title centred in the top edge. */
void tui_box(int x, int y, int w, int h, const char *title, unsigned char attr);

/* Where the caret sits. Set off-screen to hide it. */
void tui_cursor(int x, int y);

void tui_flush(void);

/* The glyph currently at a cell, as one of the G_* codes or a character.
 * Used to dump a drawn screen as text, which is how the layout is checked on
 * DOS -- there is no pty there to read an escape stream back out of. */
int  tui_cell(int x, int y);

/* Keys. Ordinary characters come back as themselves; the rest as one of
 * these, above anything a byte can hold. */
#define K_UP     0x100
#define K_DOWN   0x101
#define K_LEFT   0x102
#define K_RIGHT  0x103
#define K_HOME   0x104
#define K_END    0x105
#define K_PGUP   0x106
#define K_PGDN   0x107
#define K_DEL    0x108
#define K_F1     0x110
#define K_F2     0x111
#define K_F3     0x112
#define K_F4     0x113
#define K_F5     0x114
#define K_F6     0x115
#define K_F7     0x116
#define K_F8     0x117
#define K_F9     0x118
#define K_F10    0x119
/* A click. Where it landed is asked for separately, because threading it
 * through an int return would mean encoding three numbers in one. */
#define K_MOUSE  0x120
#define K_ESC    27
#define K_ENTER  13
#define K_BS     8
#define K_TAB    9

int tui_getkey(void);        /* blocks until a key arrives */
int tui_haskey(void);        /* non-zero if one is waiting */

/* Cell coordinates and button of the most recent K_MOUSE. Button 0 is left. */
void tui_mouse(int *x, int *y, int *button);

#endif
