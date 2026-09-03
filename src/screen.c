#include "screen.h"
#include "a2mem.h"
#include "bugs.h"

#include <stddef.h>

static scr_sink sink;
static void (*cursor_hook)(void);
static int (*filter)(char ch);
static FILE *printer;
static int card_cols = SCR_COLS;      /* what PR# last set: 40 or 80 */

/* The ROM's screen variables, read every time rather than cached, so a
 * POKE takes effect on the next character. */
#define CH      a2mem[ZP_CH]
#define CV      a2mem[ZP_CV]
#define WNDLFT  a2mem[ZP_WNDLFT]
#define WNDWDTH a2mem[ZP_WNDWDTH]
#define WNDTOP  a2mem[ZP_WNDTOP]
#define WNDBTM  a2mem[ZP_WNDBTM]

static void emit(char ch);

static void moved(void)
{
    if (cursor_hook)
        cursor_hook();
}

void scr_reset(void)
{
    WNDLFT = 0;
    WNDWDTH = (unsigned char)card_cols;
    WNDTOP = 0;
    WNDBTM = SCR_ROWS;
    CH = 0;
    CV = 0;
    a2mem[ZP_INVFLG] = 0xFF;
    a2mem[ZP_PROMPT] = (unsigned char)(']' | 0x80);   /* $DD, as the ROM kept it */
}

void scr_init(scr_sink s)
{
    sink = s;
    cursor_hook = 0;
    filter = 0;
    printer = 0;
    card_cols = SCR_COLS;
    scr_reset();
}

void scr_on_cursor(void (*hook)(void)) { cursor_hook = hook; }
void scr_notify_cursor(void) { moved(); }
void scr_set_filter(int (*f)(char)) { filter = f; }
void scr_set_printer(FILE *f) { printer = f; }
FILE *scr_printer(void) { return printer; }

/* The window, with the sanity the ROM did not have: a POKE that leaves no
 * window at all (WNDBTM below WNDTOP, a zero width) would have crashed the
 * machine or drawn off the screen, and here it just means one row or one
 * column. */
int scr_window_top(void)    { return WNDTOP < SCR_ROWS ? WNDTOP : SCR_ROWS - 1; }
int scr_window_bottom(void)
{
    int b = WNDBTM;
    if (b > SCR_ROWS) b = SCR_ROWS;
    if (b <= scr_window_top()) b = scr_window_top() + 1;
    return b;
}
int scr_window_left(void)   { return WNDLFT < SCR_MAXCOLS ? WNDLFT : SCR_MAXCOLS - 1; }
int scr_window_width(void)
{
    int w = WNDWDTH;
    if (w < 1) w = 1;
    if (scr_window_left() + w > SCR_MAXCOLS) w = SCR_MAXCOLS - scr_window_left();
    return w;
}
int scr_cols(void)      { return scr_window_width(); }
int scr_card_cols(void) { return card_cols; }

void scr_set_cols(int n)
{
    if (n != 40 && n != 80)
        return;
    if (n == card_cols)
        return;
    card_cols = n;
    /* Switching cards initialises the firmware: the whole screen clears,
     * the window is the full screen again, and the cursor is at the top. */
    WNDLFT = 0;
    WNDWDTH = (unsigned char)n;
    WNDTOP = 0;
    WNDBTM = SCR_ROWS;
    CH = 0;
    CV = 0;
    emit(SCR_CLEAR);
    moved();
}

static void emit(char ch)
{
    if (filter && filter(ch))
        return;
    if (printer) {
        if (ch == '\n') {
#ifdef __DOS__
            fputc('\r', printer);
#endif
            fputc('\n', printer);
        } else if ((unsigned char)ch >= 32) {
            fputc(ch, printer);
        }
        return;
    }
    if (sink)
        sink(ch);
}

void scr_newline(void)
{
    emit('\n');
    CH = 0;
    if (CV + 1 < scr_window_bottom())
        CV++;
    else
        CV = (unsigned char)(scr_window_bottom() - 1);
}

void scr_putc(char ch)
{
    if (ch == '\n') {
        scr_newline();
        return;
    }
    emit(ch);
    if (++CH >= scr_cols()) {
        /* The Apple wraps by itself; the cursor lands at the start of the
         * next line without a carriage return being printed by the program. */
        scr_newline();
    }
}

void scr_raw_putc(char ch)
{
    emit(ch);
}

void scr_raw_puts(const char *s)
{
    while (*s)
        emit(*s++);
}

void scr_puts(const char *s)
{
    while (*s)
        scr_putc(*s++);
}

void scr_comma(void)
{
    int target = ((CH / SCR_TABZONE) + 1) * SCR_TABZONE;
    if (target >= scr_cols()) {
        scr_newline();
        return;
    }
    while (CH < target)
        scr_putc(' ');
}

void scr_spc(int n)
{
    while (n-- > 0)
        scr_putc(' ');
}

void scr_tab(int target)
{
    /* TAB( never moves left. */
    while (CH < target)
        scr_putc(' ');
}

void scr_htab(int n)
{
    /* HTAB is 1-based, so column n-1. */
    int target = n - 1;

    if (target < 0)
        target = 0;
    if (target >= scr_cols())
        target = scr_cols() - 1;

    /* HTAB moves the cursor; it does not write anything. On the Apple the
     * screen is memory-mapped so nothing needs to be written, and a
     * streaming front end has to match that: after HTAB 10 the next PRINT
     * appears at the left of the output but the column counter reads 9, so
     * the line still wraps 31 characters later. TAB( and SPC( do print
     * spaces -- only HTAB is a pure cursor move. */
    CH = (unsigned char)target;
    moved();
}

void scr_vtab(int n)
{
    if (cursor_hook) {
        /* A real cursor: VTAB is absolute, and can go up as well as down. */
        if (n < 1) n = 1;
        if (n > SCR_ROWS) n = SCR_ROWS;
        CV = (unsigned char)(n - 1);
        moved();
        return;
    }
    /* Without a frame buffer the best a streaming front end can do is
     * move down. */
    while (CV < n - 1 && CV + 1 < scr_window_bottom())
        scr_newline();
}

void scr_home(void)
{
    CH = 0;
    CV = (unsigned char)scr_window_top();
    emit(SCR_CLEAR);
    moved();
}

void scr_clreol(void) { emit(SCR_CLREOL); }
void scr_clreop(void) { emit(SCR_CLREOP); }

void scr_window(int top)
{
    if (top < 0) top = 0;
    if (top >= SCR_ROWS) top = SCR_ROWS - 1;
    WNDTOP = (unsigned char)top;
}

void scr_window_reset(void)
{
    WNDLFT = 0;
    WNDWDTH = (unsigned char)card_cols;
    WNDTOP = 0;
    WNDBTM = SCR_ROWS;
}

void scr_cursor_bottom(void)
{
    if (!cursor_hook)
        return;
    CV = SCR_ROWS - 1;
    moved();
}

void scr_text_mode(int m)
{
    a2mem[ZP_INVFLG] = (unsigned char)(m == SCR_INVERSE ? 0x3F :
                                       m == SCR_FLASH   ? 0x7F : 0xFF);
}

int scr_get_text_mode(void)
{
    unsigned char f = a2mem[ZP_INVFLG];
    if (f & 0x80) return SCR_NORMAL;
    if (f & 0x40) return SCR_FLASH;
    return SCR_INVERSE;
}

unsigned char scr_encode(char ch)
{
    /* The ROM's COUT: set the high bit, then AND with INVFLG. */
    return (unsigned char)(((unsigned char)ch | 0x80) & a2mem[ZP_INVFLG]);
}

int  scr_col(void)    { return CH; }
int  scr_row(void)    { return CV; }
char scr_prompt(void) { return (char)(a2mem[ZP_PROMPT] & 0x7F); }
