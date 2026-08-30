#include "screen.h"
#include "bugs.h"

#include <stddef.h>

static scr_sink sink;
static int col;
static int row;
static int cols = SCR_COLS;

void scr_init(scr_sink s)
{
    sink = s;
    col = 0;
    row = 0;
    cols = SCR_COLS;
}

static void emit(char ch);

int scr_cols(void) { return cols; }

void scr_set_cols(int n)
{
    if (n != 40 && n != 80)
        return;
    if (n == cols)
        return;
    cols = n;
    /* Switching cards clears the screen and homes the cursor, as it did. */
    col = 0;
    row = 0;
    emit('\f');
}

static void emit(char ch)
{
    if (sink)
        sink(ch);
}

void scr_newline(void)
{
    emit('\n');
    col = 0;
    if (row < SCR_ROWS - 1)
        row++;
}

void scr_putc(char ch)
{
    if (ch == '\n') {
        scr_newline();
        return;
    }
    emit(ch);
    if (++col >= cols) {
        /* The Apple wraps by itself; the cursor lands at the start of the
         * next line without a carriage return being printed by the program. */
        col = 0;
        emit('\n');
        if (row < SCR_ROWS - 1)
            row++;
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
    int target = ((col / SCR_TABZONE) + 1) * SCR_TABZONE;
    if (target >= cols) {
        scr_newline();
        return;
    }
    while (col < target)
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
    while (col < target)
        scr_putc(' ');
}

void scr_htab(int n)
{
    /* HTAB is 1-based, so column n-1. The ROM lands two columns further
     * right than that, which is why HTAB 10 : PRINT starts at column 11. */
    int target = n - 1;

    if (target < 0)
        target = 0;
    if (target >= cols)
        target = cols - 1;

    /* HTAB moves the cursor; it does not write anything. On the Apple the
     * screen is memory-mapped so nothing needs to be written, and a
     * streaming front end has to match that: after HTAB 10 the next PRINT
     * appears at the left of the output but the column counter reads 11, so
     * the line still wraps 29 characters later. TAB( and SPC( do print
     * spaces -- only HTAB is a pure cursor move. */
    col = target;
}

void scr_vtab(int n)
{
    /* Without a real frame buffer the best a streaming front end can do is
     * move down; the Turbo Vision build overrides this with a true cursor. */
    while (row < n - 1)
        scr_newline();
}

void scr_home(void)
{
    col = 0;
    row = 0;
    emit('\f');
}

int scr_col(void) { return col; }
int scr_row(void) { return row; }
