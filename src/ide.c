/* ide.c - the windowed front end, drawn on tui.h.
 *
 * The layout is web/ui-mockup.html: a menu bar, the 40x24 Apple screen on the
 * left with the program listing beneath it, the Machine pane down the right,
 * and a status line. That needs 80x43, which is why the DOS backend switches
 * to the 8x8 font -- 80x25 cannot hold a 40x24 screen plus a frame, a menu bar
 * and a status line.
 *
 * The interpreter is not modified to suit this. It calls host_getline when it
 * wants a line, and that is where the event loop lives, so the same core runs
 * under the plain console front end and under this one.
 */
#include "tui.h"

#include "a2mem.h"
#include "bugs.h"
#include "host.h"
#include "interp.h"
#include "panes.h"
#include "screen.h"
#include "token.h"

#include <stdio.h>
#include <string.h>

#define APPLE_W 40
#define APPLE_H 24

/* The frames. Each is x, y, width, height; contents sit one cell inside. */
#define APPLE_X 0
#define APPLE_Y 1
#define APPLE_BW (APPLE_W + 2)
#define APPLE_BH (APPLE_H + 2)

#define PROG_X 0
#define PROG_Y (APPLE_Y + APPLE_BH)
#define PROG_BW APPLE_BW
#define PROG_BH (TUI_H - 1 - PROG_Y)
#define PROG_LINES (PROG_BH - 2)

#define MACH_X (APPLE_BW + 1)
#define MACH_Y 1
#define MACH_BW (PANE_WIDTH + 2)
#define MACH_BH (TUI_H - 1 - MACH_Y)

#define STATUS_Y (TUI_H - 1)

#define A_DESK   ATTR(C_LTBLUE, C_BLUE)
#define A_FRAME  ATTR(C_WHITE, C_BLUE)
#define A_APPLE  ATTR(C_LTGREEN, C_BLACK)
#define A_PLAIN  ATTR(C_LTGRAY, C_BLUE)
#define A_HEAD   ATTR(C_LTCYAN, C_BLUE)
#define A_WARN   ATTR(C_YELLOW, C_BLUE)
#define A_BAR    ATTR(C_BLACK, C_LTGRAY)
#define A_HOT    ATTR(C_RED, C_LTGRAY)
#define A_SEL    ATTR(C_WHITE, C_GREEN)
#define A_MENU   ATTR(C_BLACK, C_LTGRAY)

static char cells[APPLE_H][APPLE_W];
static int  arow, acol;               /* where output has reached */
static int  prog_top;                 /* first program line shown */
static char input[256];
static int  input_len;
static int  quitting;
static int  break_seen;
static char status[80] = "F5 Run   F9 List   F3 Load   F2 Save   F10 Menu   Ctrl-Q Quit";

/* ------------------------------------------------------------ apple screen */

static void apple_clear(void)
{
    int r;
    for (r = 0; r < APPLE_H; r++)
        memset(cells[r], ' ', APPLE_W);
    arow = acol = 0;
}

static void apple_scroll(void)
{
    int r;
    for (r = 0; r < APPLE_H - 1; r++)
        memcpy(cells[r], cells[r + 1], APPLE_W);
    memset(cells[APPLE_H - 1], ' ', APPLE_W);
}

static void apple_newline(void)
{
    acol = 0;
    if (arow < APPLE_H - 1) arow++;
    else apple_scroll();
}

/* This keeps its own column rather than asking the screen model for one.
 * The model's column is what POS reports, and raw output -- the "]" prompt --
 * deliberately does not advance it, so using it here would stack the whole
 * prompt into a single cell. On a real display every character takes a
 * position whether or not it counts towards POS. */
void ide_sink(char ch)
{
    if (ch == '\f') { apple_clear(); return; }
    if (ch == '\n') { apple_newline(); return; }
    if (acol >= APPLE_W)
        apple_newline();
    cells[arow][acol++] = ch;
}

/* ------------------------------------------------------------------ menus */

#define MENU_COUNT 5
static const char *menu_title[MENU_COUNT] = { "File", "Run", "Bugs", "Debug", "Help" };
static int menu_x[MENU_COUNT];

static const char *file_items[] = { "Load...", "Save...", "New", "Quit", 0 };
static const char *run_items[]  = { "Run", "List", "Clear", "Reset machine", 0 };
static const char *dbg_items[]  = { "Clear last error", "Program pane home", 0 };
static const char *help_items[] = { "About", 0 };

static const char **menu_items(int m)
{
    switch (m) {
    case 0:  return file_items;
    case 1:  return run_items;
    case 3:  return dbg_items;
    case 4:  return help_items;
    default: return 0;              /* Bugs is built fresh each time */
    }
}

static char bug_labels[BUG_COUNT][40];
static const char *bug_items[BUG_COUNT + 1];

static const char **build_bug_items(void)
{
    int i;
    for (i = 0; i < BUG_COUNT; i++) {
        sprintf(bug_labels[i], "[%c] %s", bug_enabled[i] ? 'X' : ' ', bug_name(i));
        bug_items[i] = bug_labels[i];
    }
    bug_items[BUG_COUNT] = 0;
    return bug_items;
}

static int menu_len(const char **it)
{
    int n = 0;
    while (it[n]) n++;
    return n;
}

/* ------------------------------------------------------------------ drawing */

static void draw_menubar(int active)
{
    int m, x = 2;

    tui_fill(0, 0, TUI_W, 1, ' ', A_BAR);
    for (m = 0; m < MENU_COUNT; m++) {
        const char *t = menu_title[m];
        unsigned char a = (m == active) ? A_SEL : A_BAR;
        menu_x[m] = x;
        tui_put(x - 1, 0, ' ', a);
        tui_put(x, 0, t[0], (m == active) ? a : A_HOT);
        tui_puts(x + 1, 0, t + 1, a);
        tui_put(x + (int)strlen(t) + 1, 0, ' ', a);
        x += (int)strlen(t) + 3;
    }
}

static void draw_apple(void)
{
    int y, x;
    tui_box(APPLE_X, APPLE_Y, APPLE_BW, APPLE_BH, "Apple ][", A_FRAME);
    for (y = 0; y < APPLE_H; y++)
        for (x = 0; x < APPLE_W; x++)
            tui_put(APPLE_X + 1 + x, APPLE_Y + 1 + y, cells[y][x], A_APPLE);
}

static void draw_program(void)
{
    char text[512], line[300];
    a2addr p = a2_prog_first();
    int skip, y;

    tui_box(PROG_X, PROG_Y, PROG_BW, PROG_BH, "Program", A_FRAME);
    for (skip = 0; skip < prog_top && p; skip++)
        p = a2_prog_next(p);
    for (y = 0; y < PROG_LINES; y++) {
        int i;
        for (i = 0; i < PROG_BW - 2; i++)
            tui_put(PROG_X + 1 + i, PROG_Y + 1 + y, ' ', A_PLAIN);
        if (p) {
            tok_detokenize(a2_prog_tokens(p), text, (int)sizeof(text));
            sprintf(line, "%ld %s", a2_prog_lineno(p), text);
            line[PROG_BW - 3] = '\0';
            tui_puts(PROG_X + 1, PROG_Y + 1 + y, line, A_PLAIN);
            p = a2_prog_next(p);
        }
    }
}

static void draw_machine(void)
{
    pane_line lines[PANE_MAXLINES];
    int n, y;

    tui_box(MACH_X, MACH_Y, MACH_BW, MACH_BH, "Machine", A_FRAME);
    n = pane_machine(lines, PANE_MAXLINES);
    for (y = 0; y < MACH_BH - 2; y++) {
        unsigned char a = A_PLAIN;
        int i;
        for (i = 0; i < PANE_WIDTH; i++)
            tui_put(MACH_X + 1 + i, MACH_Y + 1 + y, ' ', A_PLAIN);
        if (y >= n)
            continue;
        if (lines[y].style == PL_HEADING) a = A_HEAD;
        else if (lines[y].style == PL_WARN) a = A_WARN;
        if (lines[y].style == PL_RULE) {
            for (i = 0; i < PANE_WIDTH; i++)
                tui_put(MACH_X + 1 + i, MACH_Y + 1 + y, G_RULE, A_PLAIN);
        } else {
            tui_puts(MACH_X + 1, MACH_Y + 1 + y, lines[y].text, a);
        }
    }
}

static void draw_status(void)
{
    tui_fill(0, STATUS_Y, TUI_W, 1, ' ', A_BAR);
    tui_puts(1, STATUS_Y, status, A_BAR);
}

static void draw_all(int active_menu)
{
    tui_fill(0, 1, TUI_W, TUI_H - 2, G_SHADE, A_DESK);
    draw_menubar(active_menu);
    draw_apple();
    draw_program();
    draw_machine();
    draw_status();
}

/* Put the caret after the prompt, on the row output has reached. */
static void place_cursor(void)
{
    int col = acol + input_len;
    if (col > APPLE_W - 1) col = APPLE_W - 1;
    tui_cursor(APPLE_X + 1 + col, APPLE_Y + 1 + arow);
}

/* The line being typed is drawn over the Apple screen rather than committed
 * to it, so backspace is just an edit to the buffer. On Return the same text
 * goes through scr_putc, which is what makes it part of the transcript. */
static void overlay_input(void)
{
    int i;
    for (i = 0; i < input_len && acol + i < APPLE_W; i++)
        tui_put(APPLE_X + 1 + acol + i, APPLE_Y + 1 + arow, input[i], A_APPLE);
}

/* ------------------------------------------------------------------ dialogs */

static void dialog_box(int w, int h, const char *title, int *ox, int *oy)
{
    int x = (TUI_W - w) / 2, y = (TUI_H - h) / 2;
    tui_fill(x, y, w, h, ' ', A_MENU);
    tui_box(x, y, w, h, title, A_MENU);
    *ox = x; *oy = y;
}

static void message(const char *title, const char *text)
{
    int x, y;
    draw_all(-1);
    dialog_box(52, 7, title, &x, &y);
    tui_puts(x + 3, y + 2, text, A_MENU);
    tui_puts(x + 3, y + 4, "Press any key", A_MENU);
    tui_cursor(-1, -1);
    tui_flush();
    tui_getkey();
}

/* A one-line text prompt. Returns 0 if the user pressed Escape. */
static int ask(const char *title, const char *prompt, char *buf, int max)
{
    int x, y, len = 0, k;

    buf[0] = '\0';
    for (;;) {
        int i;
        draw_all(-1);
        dialog_box(56, 7, title, &x, &y);
        tui_puts(x + 3, y + 2, prompt, A_MENU);
        for (i = 0; i < 48; i++)
            tui_put(x + 3 + i, y + 4, ' ', ATTR(C_WHITE, C_BLACK));
        tui_puts(x + 3, y + 4, buf, ATTR(C_WHITE, C_BLACK));
        tui_cursor(x + 3 + len, y + 4);
        tui_flush();

        k = tui_getkey();
        if (k == K_ESC) return 0;
        if (k == K_ENTER) return len > 0;
        if (k == K_BS) { if (len > 0) buf[--len] = '\0'; continue; }
        if (k >= 32 && k < 127 && len < max - 1) {
            buf[len++] = (char)k;
            buf[len] = '\0';
        }
    }
}

/* ------------------------------------------------------------------ actions */

static void run_line(const char *s)
{
    scr_raw_puts(s);
    scr_newline();
    it_line(s);
}

/* Anything run from a key or a menu leaves the cursor at the start of a fresh
 * line, but the REPL's prompt was printed before the event loop was entered,
 * so there is nothing there to type against. Put one back. */
static void ensure_prompt(void)
{
    if (acol == 0)
        scr_raw_puts("]");
}

static void do_load(void)
{
    char path[128];
    if (!ask("Load", "Program to load:", path, (int)sizeof(path)))
        return;
    if (it_load(path)) {
        prog_top = 0;
        sprintf(status, "Loaded %s", path);
    } else {
        message("Load", "Could not open that file.");
    }
}

static void do_save(void)
{
    char path[128];
    if (!ask("Save", "Save program as:", path, (int)sizeof(path)))
        return;
    if (it_save(path))
        sprintf(status, "Saved %s", path);
    else
        message("Save", "Could not write that file.");
}

static void do_menu_action(int m, int item)
{
    switch (m) {
    case 0:
        if (item == 0) do_load();
        else if (item == 1) do_save();
        else if (item == 2) { run_line("NEW"); prog_top = 0; }
        else quitting = 1;
        return;
    case 1:
        if (item == 0) run_line("RUN");
        else if (item == 1) run_line("LIST");
        else if (item == 2) run_line("CLEAR");
        else { it_init(); apple_clear(); prog_top = 0; }
        return;
    case 2:
        bug_enabled[item] = (unsigned char)!bug_enabled[item];
        sprintf(status, "%s is now %s", bug_name(item),
                bug_enabled[item] ? "ON" : "OFF");
        return;
    case 3:
        if (item == 0) { a2mem[ZP_ERRNUM] = 0; strcpy(status, "Last error cleared"); }
        else prog_top = 0;
        return;
    default:
        message("About",
                "Applesoft BASIC for DOS - the ROM's bugs, on purpose.");
        return;
    }
}

/* Drop one menu down and let the user pick. Returns when it closes. */
static void menu_loop(void)
{
    int m = 0, item = 0;

    for (;;) {
        const char **items = (m == 2) ? build_bug_items() : menu_items(m);
        int n = menu_len(items), w = 0, i, k, x, y;

        for (i = 0; i < n; i++)
            if ((int)strlen(items[i]) > w) w = (int)strlen(items[i]);
        w += 4;
        x = menu_x[m] - 1;
        if (x + w >= TUI_W) x = TUI_W - w - 1;
        y = 1;

        draw_all(m);
        tui_fill(x, y, w, n + 2, ' ', A_MENU);
        tui_box(x, y, w, n + 2, 0, A_MENU);
        for (i = 0; i < n; i++)
            tui_puts(x + 2, y + 1 + i, items[i],
                     (i == item) ? A_SEL : A_MENU);
        tui_cursor(-1, -1);
        tui_flush();

        k = tui_getkey();
        if (k == K_ESC) return;
        if (k == K_LEFT)  { m = (m + MENU_COUNT - 1) % MENU_COUNT; item = 0; continue; }
        if (k == K_RIGHT) { m = (m + 1) % MENU_COUNT; item = 0; continue; }
        if (k == K_UP)    { item = (item + n - 1) % n; continue; }
        if (k == K_DOWN)  { item = (item + 1) % n; continue; }
        if (k == K_ENTER) {
            do_menu_action(m, item);
            if (m == 2) continue;         /* toggles stay open, as TV's did */
            return;
        }
    }
}

/* ------------------------------------------------------------- the host API */

static int pump(int want_line, char *buf, int max)
{
    for (;;) {
        int k;

        draw_all(-1);
        overlay_input();
        place_cursor();
        tui_flush();

        k = tui_getkey();

        if (k == K_F10) { menu_loop(); ensure_prompt(); continue; }
        if (k == 17) { quitting = 1; return 0; }            /* Ctrl-Q */
        if (k == 3)  { break_seen = 1; continue; }          /* Ctrl-C */
        if (k == K_F5) { run_line("RUN"); ensure_prompt(); continue; }
        if (k == K_F9) { run_line("LIST"); ensure_prompt(); continue; }
        if (k == K_F3) { do_load(); ensure_prompt(); continue; }
        if (k == K_F2) { do_save(); ensure_prompt(); continue; }
        if (k == K_PGUP) { if (prog_top > 0) prog_top--; continue; }
        if (k == K_PGDN) { prog_top++; continue; }

        if (k == 10) k = K_ENTER;      /* a terminal that translated it anyway */

        if (!want_line) {
            if (k >= 32 && k < 127) return k;
            if (k == K_ENTER) return 13;
            continue;
        }

        if (k == K_ENTER) {
            int i;
            input[input_len] = '\0';
            /* Commit the typed line to the transcript exactly as the Apple
             * would have: the characters, then the return. */
            for (i = 0; i < input_len; i++)
                scr_putc(input[i]);
            scr_newline();
            strncpy(buf, input, (size_t)max - 1);
            buf[max - 1] = '\0';
            input_len = 0;
            return 1;
        }
        if (k == K_BS) { if (input_len > 0) input_len--; continue; }
        if (k >= 32 && k < 127 && input_len < (int)sizeof(input) - 1)
            input[input_len++] = (char)k;
    }
}

int host_getline(char *buf, int max)
{
    if (quitting) return 0;
    return pump(1, buf, max);
}

int host_getkey(void)
{
    if (quitting) return 0;
    return pump(0, 0, 0);
}

int host_break(void)
{
    /* Poll without blocking, so a long FOR loop can still be interrupted. */
    while (tui_haskey()) {
        int k = tui_getkey();
        if (k == 3) break_seen = 1;
    }
    if (break_seen) { break_seen = 0; return 1; }
    return 0;
}

int ide_quitting(void) { return quitting; }

/* Draw the whole layout once, without waiting for a key. */
void ide_draw_once(void)
{
    draw_all(-1);
    tui_cursor(-1, -1);
    tui_flush();
}

void ide_start(void)
{
    apple_clear();
    prog_top = 0;
    input_len = 0;
    quitting = 0;
}
