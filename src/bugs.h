/* bugs.h - the ROM behaviours you can switch off.
 *
 * Applesoft has a handful of famous misfeatures that programs of the period
 * relied on, or tripped over. Reproducing them is the point of this
 * interpreter, so they are on by default; each one can be turned off
 * individually, which is what the Bugs menu in the Turbo Vision front end
 * drives. Turning one off makes the interpreter more sensible and less
 * accurate, which is exactly the trade the menu is offering. */
#ifndef ASOFT_BUGS_H
#define ASOFT_BUGS_H

/* ONERR GOTO leaks a stack frame on every trapped error, so a program that
 * traps errors in a loop dies of OUT OF MEMORY. CALL -3288 pops the frame,
 * which is what ONERRFIX.BAS demonstrates. */
#define BUG_ONERR_LEAK      0

/* HTAB n leaves the cursor two columns right of where it should: HTAB 10
 * puts it at column 11 (0-based), not 9. POS then honestly reports 11. */
#define BUG_HTAB_OFFBYTWO   1

/* Every arithmetic result is rounded to MBF's 32-bit mantissa. Switching it
 * off keeps full host precision, so .1 + .2 stops printing as .3. */
#define BUG_MBF_ROUNDING    2

/* Keywords are matched anywhere, so TOTAL tokenizes as TO + TAL. */
#define BUG_GREEDY_TOKENIZER 3

#define BUG_COUNT 4

extern unsigned char bug_enabled[BUG_COUNT];
const char *bug_name(int which);

#endif
