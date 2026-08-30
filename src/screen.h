/* screen.h - the 40-column Apple ][ text screen.
 *
 * Applesoft's output is inseparable from the screen it was written for: PRINT
 * wraps at column 40, commas step through 16-column zones, and POS reports the
 * cursor column, so a program's output depends on the geometry. Both front
 * ends drive this same model and differ only in where the characters land.
 */
#ifndef ASOFT_SCREEN_H
#define ASOFT_SCREEN_H

/* The width is not fixed. An Apple with an 80-column card in slot 3 becomes
 * an eighty-column machine when a program says PR#3, and goes back to forty
 * on PR#0 -- so the wrap point, HTAB's limit and the comma zones all move.
 * SCR_MAXCOLS is what a buffer has to be able to hold. */
#define SCR_MAXCOLS 80
#define SCR_COLS 40              /* the width it starts at */
#define SCR_ROWS 24

int  scr_cols(void);
void scr_set_cols(int n);        /* 40 or 80; anything else is ignored */
#define SCR_TABZONE 16

/* Supplied by the front end: put one character on the real display. The
 * screen model has already done wrapping, so ch is never a newline except
 * when scr_newline decided one was due. */
typedef void (*scr_sink)(char ch);

void scr_init(scr_sink sink);
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
void scr_home(void);
int  scr_col(void);              /* 0-based; what POS(0) reports */
int  scr_row(void);

#endif
