/* screen.h - the Apple ][ text screen, as a model.
 *
 * Applesoft's output is inseparable from the screen it was written for: PRINT
 * wraps at the window's edge, commas step through 16-column zones, and POS
 * reports the cursor column, so a program's output depends on the geometry.
 * Every front end drives this same model and differs only in where the
 * characters land.
 *
 * The state is the ROM's, in zero page: CH and CV for the cursor, WNDLFT,
 * WNDWDTH, WNDTOP and WNDBTM for the window, INVFLG for NORMAL/INVERSE/FLASH,
 * PROMPT for the prompt character. That is not for show. Programs POKEd
 * these -- POKE 34,20 to keep a status line clear of the scroll, POKE 36,X
 * to move the cursor, POKE 50,63 for inverse -- and read them back, and all
 * of that works only if the numbers there are the numbers in use.
 *
 * What the model is not is the page memory: a front end that has one (the
 * DOS console, which keeps the text page at $400 exactly as the machine
 * did) puts the characters there itself and follows the cursor moves
 * through the hook below; a front end that only has a stream gets the
 * characters and does what it can with them.
 */
#ifndef ASOFT_SCREEN_H
#define ASOFT_SCREEN_H

#include <stdio.h>

/* The width is not fixed. An Apple with an 80-column card in slot 3 becomes
 * an eighty-column machine when a program says PR#3, and goes back to forty
 * on PR#0 -- so the wrap point, HTAB's limit and the comma zones all move.
 * SCR_MAXCOLS is what a buffer has to be able to hold. */
#define SCR_MAXCOLS 80
#define SCR_COLS 40              /* the width it starts at */
#define SCR_ROWS 24

/* The window's width, which is what PRINT wraps at. Programs narrow it with
 * POKE 33,n. */
int  scr_cols(void);
/* The card's width, 40 or 80: what the front end's screen is. */
int  scr_card_cols(void);
void scr_set_cols(int n);        /* PR#0 / PR#3: 40 or 80; anything else is ignored */
#define SCR_TABZONE 16

/* Supplied by the front end: put one character on the real display. The
 * screen model has already done wrapping, so ch is never a newline except
 * when scr_newline decided one was due. A few control characters ask for
 * screen operations the ROM had entry points for: */
#define SCR_CLEAR   '\f'         /* clear the text window and home (HOME) */
#define SCR_CLREOL  '\v'         /* clear from the cursor to the end of the line */
#define SCR_CLREOP  0x1A         /* clear from the cursor to the end of the window */
typedef void (*scr_sink)(char ch);

void scr_init(scr_sink sink);
/* Put the ROM's defaults back into zero page: the full screen, the cursor
 * at the top, NORMAL, "]". After a2_init has wiped the image. */
void scr_reset(void);

/* Optional, for a front end with a real cursor: called whenever the cursor
 * moves without anything being printed -- HTAB, VTAB, HOME, a POKE into 36
 * or 37, and the mode switches that park it on the bottom line. The front
 * end reads scr_row and scr_col to find out where it went. Without one,
 * VTAB can only move down, by printing newlines, because a stream has no
 * way back up. */
void scr_on_cursor(void (*hook)(void));
void scr_notify_cursor(void);    /* something POKEd the cursor or window */

/* Everything the model emits goes through the filter first, when there is
 * one: the DOS command channel, watching for a control-D at the start of a
 * line and swallowing what follows it. Non-zero from the filter means it
 * took the character. */
void scr_set_filter(int (*filter)(char ch));

/* PR#1: while a printer is set, output goes there and not to the screen. */
void scr_set_printer(FILE *f);
FILE *scr_printer(void);

void scr_putc(char ch);
/* Write without advancing the column. The reference build's "]" prompt
 * does not count towards the 40-column wrap or towards POS: after a
 * prompt, PRINT 1,2,3 still puts the 2 at column 16, not 17. */
void scr_raw_putc(char ch);
void scr_raw_puts(const char *s);
void scr_puts(const char *s);
void scr_newline(void);
void scr_comma(void);            /* advance to the next 16-column zone */
void scr_spc(int n);
void scr_tab(int col);           /* TAB(n): move right to column n */
void scr_htab(int n);            /* HTAB n */
void scr_vtab(int n);
void scr_home(void);             /* HOME, CALL -936 */
void scr_clreol(void);           /* CALL -868 */
void scr_clreop(void);           /* CALL -958 */
int  scr_col(void);              /* 0-based, within the window; what POS(0) reports */
int  scr_row(void);              /* 0-based, absolute */
char scr_prompt(void);           /* what the prompt prints: "]" unless POKE 51 */

/* The text window. GR sets its top to row 20, so printing and scrolling
 * only touch the four lines under the picture; TEXT puts the whole screen
 * back. Programs set all four edges with POKEs. */
void scr_window(int top);
void scr_window_reset(void);     /* the whole screen: what TEXT does */
int  scr_window_top(void);
int  scr_window_bottom(void);    /* one past the last row */
int  scr_window_left(void);
int  scr_window_width(void);

/* What the ROM's TEXT and GR both do after setting the switches: leave the
 * column alone and move the cursor to the bottom line. Only a front end
 * with a cursor hook can follow it; for a stream it is a no-op, since the
 * only way a stream could move down is by printing, and that would put
 * twenty blank lines in every transcript. */
void scr_cursor_bottom(void);

/* NORMAL, INVERSE, FLASH: how the next characters are stored in the page.
 * Kept as the ROM kept it, a mask at INVFLG; the front end with a page
 * ANDs each character with it, which is exactly what the ROM did. */
#define SCR_NORMAL  0
#define SCR_INVERSE 1
#define SCR_FLASH   2
void scr_text_mode(int m);
int  scr_get_text_mode(void);
unsigned char scr_encode(char ch);   /* a character as the page stores it */

#endif
