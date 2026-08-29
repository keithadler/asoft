/* layout.c - draw the Turbo Vision screen as text, from live state.
 *
 * The Turbo Vision build itself needs Borland C++ under DOS to compile, which
 * makes it awkward to check anything about it from here. So the layout and the
 * pane contents are worked out in portable C, drawn to stdout, and compared
 * against web/ui-mockup.html. What main_tv.cpp then does is put these same
 * strings inside real TWindows.
 *
 *   80x43, which is why the app switches to the 8x8 font: a 40-column Apple
 *   screen plus a frame, a menu bar, a status line and two side panes does not
 *   fit in 80x25.
 *
 * usage: layout [-c] [program.bas]     -c adds ANSI colour
 */
#include "../src/a2mem.h"
#include "../src/bugs.h"
#include "../src/host.h"
#include "../src/interp.h"
#include "../src/panes.h"
#include "../src/screen.h"
#include "../src/token.h"

#include <stdio.h>
#include <string.h>

#define COLS 80
#define ROWS 43
#define APPLE_W 40                 /* the Apple screen itself */
#define MACH_W  35

/* The Apple ][ pane is a scrollback of what the program printed. */
#define VIEW_ROWS 24
static char view[VIEW_ROWS][APPLE_W + 1];
static int  view_col;

static void view_newline(void)
{
    int r;
    for (r = 0; r < VIEW_ROWS - 1; r++)
        strcpy(view[r], view[r + 1]);
    memset(view[VIEW_ROWS - 1], ' ', APPLE_W);
    view[VIEW_ROWS - 1][APPLE_W] = '\0';
    view_col = 0;
}

static void sink(char ch)
{
    if (ch == '\f') {
        int r;
        for (r = 0; r < VIEW_ROWS; r++) {
            memset(view[r], ' ', APPLE_W);
            view[r][APPLE_W] = '\0';
        }
        view_col = 0;
        return;
    }
    if (ch == '\n') { view_newline(); return; }
    if (view_col < APPLE_W)
        view[VIEW_ROWS - 1][view_col++] = ch;
}

int host_getline(char *buf, int max) { (void)buf; (void)max; return 0; }
int host_getkey(void) { return 0; }
int host_break(void) { return 0; }

/* --------------------------------------------------------------- drawing */

static char screen[ROWS][COLS + 1];
static unsigned char attr[ROWS][COLS];

#define A_DESK  0
#define A_MENU  1
#define A_FRAME 2
#define A_TITLE 3
#define A_APPLE 4
#define A_SHADOW 5
#define A_HEAD  6
#define A_WARN  7
#define A_PROG  8
#define A_HOT   9

static void put(int r, int c, const char *s, int a)
{
    while (*s && c < COLS) {
        screen[r][c] = *s++;
        attr[r][c] = (unsigned char)a;
        c++;
    }
}

static void frame(int r0, int c0, int h, int w, const char *title, int a)
{
    int r, c;
    char buf[COLS + 1];
    int tlen = (int)strlen(title);
    int tstart = c0 + (w - tlen - 2) / 2;

    for (c = 0; c < w; c++) { screen[r0][c0 + c] = '='; attr[r0][c0 + c] = (unsigned char)a; }
    screen[r0][c0] = '+'; screen[r0][c0 + w - 1] = '+';
    put(r0, c0 + 1, "[*]", a);
    if (tlen) {
        sprintf(buf, " %s ", title);
        put(r0, tstart, buf, A_TITLE);
    }
    for (r = 1; r < h - 1; r++) {
        screen[r0 + r][c0] = '|'; attr[r0 + r][c0] = (unsigned char)a;
        screen[r0 + r][c0 + w - 1] = '|'; attr[r0 + r][c0 + w - 1] = (unsigned char)a;
    }
    for (c = 0; c < w; c++) { screen[r0 + h - 1][c0 + c] = '='; attr[r0 + h - 1][c0 + c] = (unsigned char)a; }
    screen[r0 + h - 1][c0] = '+'; screen[r0 + h - 1][c0 + w - 1] = '+';
}

static const char *ansi(int a)
{
    switch (a) {
    case A_MENU:   return "\033[30;47m";
    case A_HOT:    return "\033[31;47m";
    case A_FRAME:  return "\033[37;44m";
    case A_TITLE:  return "\033[1;33;44m";
    case A_APPLE:  return "\033[32;40m";
    case A_SHADOW: return "\033[34;44m";
    case A_HEAD:   return "\033[36;44m";
    case A_WARN:   return "\033[33;44m";
    case A_PROG:   return "\033[36;44m";
    default:       return "\033[37;44m";
    }
}

int main(int argc, char **argv)
{
    int colour = 0, i, r, c;
    const char *path = 0;
    pane_line pl[PANE_MAXLINES];
    int npl;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) colour = 1;
        else path = argv[i];
    }

    for (r = 0; r < VIEW_ROWS; r++) {
        memset(view[r], ' ', APPLE_W);
        view[r][APPLE_W] = '\0';
    }
    scr_init(sink);
    it_init();

    sink('A'); /* banner, the way the real front end greets you */
    {
        const char *banner = "PPLESOFT BASIC FOR DOS";
        while (*banner) sink(*banner++);
        sink('\n'); sink('\n');
    }

    if (path) {
        if (!it_load(path)) { fprintf(stderr, "cannot load %s\n", path); return 1; }
        { const char *s = "]LOAD "; while (*s) sink(*s++); }
        { const char *s = path; while (*s) sink(*s++); }
        sink('\n');
        { const char *s = "]RUN"; while (*s) sink(*s++); }
        sink('\n');
        it_line("RUN");
    }
    { const char *s = "]"; while (*s) sink(*s++); }

    for (r = 0; r < ROWS; r++) {
        memset(screen[r], ' ', COLS);
        screen[r][COLS] = '\0';
        memset(attr[r], A_DESK, COLS);
    }

    /* menu bar */
    for (c = 0; c < COLS; c++) attr[0][c] = A_MENU;
    put(0, 2, "File  Run  Bugs  Debug  Help", A_MENU);
    attr[0][2] = A_HOT; attr[0][8] = A_HOT; attr[0][15] = A_HOT; attr[0][21] = A_HOT; attr[0][28] = A_HOT;

    /* Apple ][ window, 40 columns of screen plus a frame */
    frame(1, 0, VIEW_ROWS + 2, APPLE_W + 2, "Apple ][", A_FRAME);
    for (r = 0; r < VIEW_ROWS; r++)
        put(2 + r, 1, view[r], A_APPLE);
    for (r = 2; r < 2 + VIEW_ROWS + 1; r++) {
        screen[r][APPLE_W + 2] = '.';
        attr[r][APPLE_W + 2] = A_SHADOW;
    }

    /* Machine pane */
    frame(1, APPLE_W + 3, VIEW_ROWS + 2, MACH_W + 2, "Machine", A_FRAME);
    npl = pane_machine(pl, PANE_MAXLINES);
    for (i = 0; i < npl && i < VIEW_ROWS; i++) {
        int a = pl[i].style == PL_HEADING ? A_HEAD :
                pl[i].style == PL_WARN ? A_WARN : A_FRAME;
        if (pl[i].style == PL_RULE) {
            char rule[MACH_W + 1];
            memset(rule, '-', MACH_W);
            rule[MACH_W] = '\0';
            put(2 + i, APPLE_W + 4, rule, A_FRAME);
        } else {
            put(2 + i, APPLE_W + 5, pl[i].text, a);
        }
    }

    /* Program pane */
    frame(VIEW_ROWS + 3, 0, ROWS - VIEW_ROWS - 4, COLS, "Program", A_FRAME);
    {
        a2addr p = a2_prog_first();
        char buf[512];
        int row = VIEW_ROWS + 4;
        int last = ROWS - 3;
        for (; p && row < last; p = a2_prog_next(p), row++) {
            char line[COLS];
            tok_detokenize(a2_prog_tokens(p), buf, (int)sizeof(buf));
            sprintf(line, "%d %s", a2_prog_lineno(p), buf);
            line[COLS - 4] = '\0';
            put(row, 2, line, A_PROG);
        }
    }

    /* status line */
    for (c = 0; c < COLS; c++) attr[ROWS - 1][c] = A_MENU;
    put(ROWS - 1, 1,
        "Ctrl-R Run   Ctrl-C Break   F3 Load   F2 Save   Alt-X Exit", A_MENU);

    for (r = 0; r < ROWS; r++) {
        if (!colour) {
            printf("%s\n", screen[r]);
            continue;
        }
        for (c = 0; c < COLS; c++) {
            if (c == 0 || attr[r][c] != attr[r][c - 1])
                fputs(ansi(attr[r][c]), stdout);
            putchar(screen[r][c]);
        }
        fputs("\033[0m\n", stdout);
    }
    return 0;
}
