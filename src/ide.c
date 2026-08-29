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

/* The layout owes more to a modern terminal application than to 1990: no
 * frames, no desktop pattern, sections separated by space and a coloured
 * heading. Everything is placed against these numbers rather than nested in
 * boxes, so moving a section is one edit.
 */
#define MENU_Y      0
#define CHIP_Y      1
#define HEAD_Y      3

#define APPLE_W     40
#define APPLE_H     24
#define APPLE_X     1
#define APPLE_Y     4

#define MACH_X      44
#define MACH_Y      4
#define MACH_H      (TUI_H - 1 - MACH_Y)

#define PROG_HEAD_Y 29
#define PROG_X      1
#define PROG_W      40
#define PROG_Y      30
#define PROG_LINES  (TUI_H - 1 - PROG_Y)

#define STATUS_Y    (TUI_H - 1)

#define A_BG     ATTR(C_TEXT,   C_BG)
#define A_DIM    ATTR(C_DIM,    C_BG)
#define A_HEAD   ATTR(C_CYAN,   C_BG)
#define A_KEY    ATTR(C_CYAN,   C_BG)
#define A_CHIP   ATTR(C_BG,     C_CYAN)
#define A_APPLE  ATTR(C_GREEN,  C_PANEL)
#define A_PLAIN  ATTR(C_TEXT,   C_BG)
#define A_WARN   ATTR(C_YELLOW, C_BG)
#define A_SEL    ATTR(C_BRIGHT, C_SEL)
#define A_MENU   ATTR(C_TEXT,   C_PANEL)
#define A_MENUSEL ATTR(C_BRIGHT, C_SEL)
#define A_EDIT   ATTR(C_BRIGHT, C_SEL)

static char cells[APPLE_H][APPLE_W];
static int  arow, acol;               /* where output has reached */
static int  prog_top;                 /* first program line shown */
static char input[256];
static int  input_len;
static int  quitting;
static int  break_seen;
static char status[80];
static char loaded[40];               /* shown in the identity strip */

/* Which pane the keyboard is talking to. The prompt is the Apple as it was:
 * type a numbered line and it is stored. The editor is what an IDE is for --
 * move around the listing and change it in place. Tab swaps between them. */
#define FOCUS_TERM 0
#define FOCUS_EDIT 1
static int  focus = FOCUS_TERM;

/* The editor works one line at a time: the line under the cursor is
 * detokenized into ed_buf, edited there, and handed back to the interpreter
 * as though it had been typed. That keeps a single path into the program --
 * it_line stores it, exactly as the prompt does -- so there is no second
 * implementation of what a line means. */
static int  ed_row, ed_col;
static char ed_buf[256];
static int  ed_dirty;


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

/* ------------------------------------------------------------------ editor */

static int prog_count(void)
{
    a2addr p = a2_prog_first();
    int n = 0;
    while (p) { n++; p = a2_prog_next(p); }
    return n;
}

static a2addr prog_at(int i)
{
    a2addr p = a2_prog_first();
    while (i-- > 0 && p)
        p = a2_prog_next(p);
    return p;
}

/* "10 PRINT ..." as the listing shows it. */
static void prog_text(int i, char *out, int max)
{
    char body[512];
    a2addr p = prog_at(i);

    out[0] = '\0';
    if (!p)
        return;
    tok_detokenize(a2_prog_tokens(p), body, (int)sizeof(body));
    sprintf(out, "%ld %s", a2_prog_lineno(p), body);
    out[max - 1] = '\0';
}

static void ed_load(void)
{
    prog_text(ed_row, ed_buf, (int)sizeof(ed_buf));
    ed_dirty = 0;
    if (ed_col > (int)strlen(ed_buf))
        ed_col = (int)strlen(ed_buf);
}

/* Hand the edited line back. Storing a line can move every line after it, so
 * the caller re-finds its place afterwards. */
static void ed_commit(void)
{
    const char *p = ed_buf;

    if (!ed_dirty)
        return;
    ed_dirty = 0;
    while (*p == ' ')
        p++;
    if (!*p)
        return;
    /* A line in the listing has to start with a number. Without this a line
     * typed without one would be run as an immediate statement the moment the
     * cursor left it, which is a surprising way to lose an edit. */
    if (*p < '0' || *p > '9') {
        strcpy(status, "A program line has to start with a line number");
        return;
    }
    it_line(ed_buf);
}

/* Keep the cursor's line on screen. */
static void ed_scroll_into_view(void)
{
    if (ed_row < prog_top)
        prog_top = ed_row;
    if (ed_row >= prog_top + PROG_LINES)
        prog_top = ed_row - PROG_LINES + 1;
    if (prog_top < 0)
        prog_top = 0;
}

static void ed_move(int delta)
{
    int n;
    ed_commit();
    n = prog_count();
    ed_row += delta;
    if (ed_row < 0) ed_row = 0;
    if (ed_row > n) ed_row = n;      /* one past the end: a new line */
    ed_load();
    ed_scroll_into_view();
}

/* Applesoft deletes a line by giving its number alone. */
static void ed_delete_line(void)
{
    a2addr p = prog_at(ed_row);
    char num[32];

    if (!p)
        return;
    sprintf(num, "%ld", a2_prog_lineno(p));
    ed_dirty = 0;
    it_line(num);
    if (ed_row > prog_count())
        ed_row = prog_count();
    ed_load();
    ed_scroll_into_view();
}

static void ed_insert(int ch)
{
    int len = (int)strlen(ed_buf);
    int i;

    if (len >= (int)sizeof(ed_buf) - 1)
        return;
    for (i = len; i >= ed_col; i--)
        ed_buf[i + 1] = ed_buf[i];
    ed_buf[ed_col++] = (char)ch;
    ed_dirty = 1;
}

static void ed_backspace(void)
{
    int len = (int)strlen(ed_buf);
    int i;

    if (ed_col <= 0)
        return;
    for (i = ed_col - 1; i < len; i++)
        ed_buf[i] = ed_buf[i + 1];
    ed_col--;
    ed_dirty = 1;
}

static void ed_del(void)
{
    int len = (int)strlen(ed_buf);
    int i;

    if (ed_col >= len)
        return;
    for (i = ed_col; i < len; i++)
        ed_buf[i] = ed_buf[i + 1];
    ed_dirty = 1;
}

/* ------------------------------------------------------------------ menus */

#define MENU_COUNT 6
static const char *menu_title[MENU_COUNT] = {
    "File", "Run", "Bugs", "Debug", "Samples", "Help"
};
static int menu_x[MENU_COUNT];

static const char *file_items[] = { "Load...", "Save...", "New", "Quit", 0 };
static const char *run_items[]  = { "Run", "List", "Clear", "Reset machine", 0 };
static const char *dbg_items[]  = { "Clear last error", "Program pane home", 0 };
static const char *help_items[] = { "About", 0 };

/* The programs that ship in the bundle, so they are one pick away rather
 * than something you have to know the name of. */
static const char *sample_items[] = {
    "Tests - compatibility suite",
    "Moire - interference",
    "Spirograph",
    "Sierpinski",
    "Mandelbrot",
    "Hi-res colour demo",
    "Shape tables",
    "Lo-res colours",
    "Snake - a game",
    "ONERR leak",
    0
};
static const char *sample_file[] = {
    "TESTS.BAS", "MOIRE.BAS", "SPIRO.BAS", "SIERP.BAS", "MANDEL.BAS",
    "HGRDEMO.BAS", "HGRSHAP.BAS", "LORES.BAS", "SNAKE.BAS", "ONERRFIX.BAS"
};

static const char **menu_items(int m)
{
    switch (m) {
    case 0:  return file_items;
    case 1:  return run_items;
    case 3:  return dbg_items;
    case 4:  return sample_items;
    case 5:  return help_items;
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
    int m, x = 1;

    tui_fill(0, MENU_Y, TUI_W, 1, ' ', A_BG);
    for (m = 0; m < MENU_COUNT; m++) {
        const char *t = menu_title[m];
        int len = (int)strlen(t);
        unsigned char a = (m == active) ? A_SEL : A_DIM;
        menu_x[m] = x + 1;
        tui_put(x, MENU_Y, ' ', a);
        tui_puts(x + 1, MENU_Y, t, a);
        tui_put(x + len + 1, MENU_Y, ' ', a);
        x += len + 4;
    }
}

/* The identity strip: a chip, what this is, and what is loaded. */
static void draw_chip(void)
{
    const char *name = loaded[0] ? loaded : "(no program)";
    int len = (int)strlen(name);

    tui_fill(0, CHIP_Y, TUI_W, 1, ' ', A_BG);
    tui_puts(1, CHIP_Y, " asoft ", A_CHIP);
    tui_puts(10, CHIP_Y, "Applesoft BASIC for DOS", A_DIM);
    if (len < TUI_W - 2)
        tui_puts(TUI_W - 1 - len, CHIP_Y, name, A_DIM);
}

/* A heading: the name in the accent colour, a note beside it in grey. */
static void heading(int x, int y, const char *name, const char *note)
{
    tui_puts(x, y, name, A_HEAD);
    if (note)
        tui_puts(x + (int)strlen(name) + 2, y, note, A_DIM);
}

static void draw_apple(void)
{
    int y, x;

    heading(APPLE_X, HEAD_Y, "Apple ][", "40x24");
    for (y = 0; y < APPLE_H; y++) {
        for (x = 0; x < APPLE_W; x++)
            tui_put(APPLE_X + x, APPLE_Y + y, cells[y][x], A_APPLE);
    }
}

static void draw_program(void)
{
    char line[300], note[32];
    int n = prog_count();
    int y;

    sprintf(note, "%d line%s", n, n == 1 ? "" : "s");
    heading(PROG_X, PROG_HEAD_Y, "Program",
            (focus == FOCUS_EDIT) ? "editing" : note);

    for (y = 0; y < PROG_LINES; y++) {
        int row = prog_top + y;
        int editing = (focus == FOCUS_EDIT && row == ed_row);
        unsigned char a = editing ? A_EDIT : A_PLAIN;
        unsigned char na = editing ? A_EDIT : A_DIM;
        int i, col;

        for (i = 0; i < PROG_W; i++)
            tui_put(PROG_X + i, PROG_Y + y, ' ', editing ? A_EDIT : A_BG);
        if (row > n)
            continue;
        if (editing) {
            strncpy(line, ed_buf, sizeof(line) - 1);
            line[sizeof(line) - 1] = '\0';
        } else if (row < n) {
            prog_text(row, line, (int)sizeof(line));
        } else {
            continue;
        }
        line[PROG_W - 1] = '\0';

        /* The line number is secondary; the code is what you are reading. */
        for (col = 0; line[col] && line[col] >= '0' && line[col] <= '9'; col++)
            tui_put(PROG_X + col, PROG_Y + y, line[col], na);
        tui_puts(PROG_X + col, PROG_Y + y, line + col, a);
    }
}

static void draw_machine(void)
{
    pane_line lines[PANE_MAXLINES];
    int n, y;

    heading(MACH_X, HEAD_Y, "Machine", 0);
    n = pane_machine(lines, PANE_MAXLINES);
    for (y = 0; y < MACH_H; y++) {
        unsigned char a = A_PLAIN;
        int i;

        for (i = 0; i < PANE_WIDTH; i++)
            tui_put(MACH_X + i, MACH_Y + y, ' ', A_BG);
        if (y >= n)
            continue;
        if (lines[y].style == PL_HEADING) a = A_HEAD;
        else if (lines[y].style == PL_WARN) a = A_WARN;
        if (lines[y].style == PL_RULE) {
            for (i = 0; i < PANE_WIDTH; i++)
                tui_put(MACH_X + i, MACH_Y + y, G_RULE, A_DIM);
        } else {
            tui_puts(MACH_X, MACH_Y + y, lines[y].text, a);
        }
    }
}

/* "F5 Run" with the key in the accent colour and the word beside it in grey,
 * which is the whole of what a status bar has to do. */
static void draw_status(void)
{
    static const char *term_keys[] = {
        "Tab", "Edit", "F5", "Run", "F9", "List", "F3", "Load",
        "F2", "Save", "F10", "Menu", "^Q", "Quit", 0
    };
    static const char *edit_keys[] = {
        "Tab", "Prompt", "F5", "Run", "F8", "Delete", "F10", "Menu",
        "^Q", "Quit", 0
    };
    const char **k = (focus == FOCUS_EDIT) ? edit_keys : term_keys;
    int x = 1, i;

    tui_fill(0, STATUS_Y, TUI_W, 1, ' ', A_BG);
    if (status[0]) {
        tui_puts(1, STATUS_Y, status, A_WARN);
        return;
    }
    for (i = 0; k[i]; i += 2) {
        tui_puts(x, STATUS_Y, k[i], A_KEY);
        x += (int)strlen(k[i]) + 1;
        tui_puts(x, STATUS_Y, k[i + 1], A_DIM);
        x += (int)strlen(k[i + 1]) + 3;
    }
}

static void draw_all(int active_menu)
{
    tui_clear(A_BG);
    draw_menubar(active_menu);
    draw_chip();
    draw_apple();
    draw_program();
    draw_machine();
    draw_status();
}

/* Put the caret after the prompt, on the row output has reached. */
static void place_cursor(void)
{
    int col;

    if (focus == FOCUS_EDIT) {
        col = ed_col;
        if (col > PROG_W - 1) col = PROG_W - 1;
        tui_cursor(PROG_X + col, PROG_Y + (ed_row - prog_top));
        return;
    }
    col = acol + input_len;
    if (col > APPLE_W - 1) col = APPLE_W - 1;
    tui_cursor(APPLE_X + col, APPLE_Y + arow);
}

/* The line being typed is drawn over the Apple screen rather than committed
 * to it, so backspace is just an edit to the buffer. On Return the same text
 * goes through scr_putc, which is what makes it part of the transcript. */
static void overlay_input(void)
{
    int i;
    if (focus != FOCUS_TERM)
        return;
    for (i = 0; i < input_len && acol + i < APPLE_W; i++)
        tui_put(APPLE_X + acol + i, APPLE_Y + arow, input[i], A_APPLE);
}

/* ------------------------------------------------------------------ dialogs */

/* A panel, not a framed box: a lifted surface with its name along the top. */
static void dialog_box(int w, int h, const char *title, int *ox, int *oy)
{
    int x = (TUI_W - w) / 2, y = (TUI_H - h) / 2;
    tui_fill(x, y, w, h, ' ', A_MENU);
    tui_puts(x + 2, y, title, ATTR(C_CYAN, C_PANEL));
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

void ide_set_loaded(const char *name);

static void do_load(void)
{
    char path[128];
    if (!ask("Load", "Program to load:", path, (int)sizeof(path)))
        return;
    if (it_load(path)) {
        prog_top = 0;
        ide_set_loaded(path);
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

/* A sample sits beside the binary on DOS and under web/bundle in the source
 * tree, so try both rather than making the caller care which. */
static void load_sample(int i)
{
    char path[64];

    if (it_load(sample_file[i])) {
        ide_set_loaded(sample_file[i]);
    } else {
        sprintf(path, "web/bundle/%s", sample_file[i]);
        if (!it_load(path)) {
            sprintf(status, "Could not find %s", sample_file[i]);
            return;
        }
        ide_set_loaded(path);
    }
    prog_top = 0;
    sprintf(status, "Loaded %s - press F5 to run", sample_file[i]);
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
    case 4:
        load_sample(item);
        return;
    default:
        message("About",
                "Applesoft BASIC for DOS - the ROM's bugs, on purpose.");
        return;
    }
}

/* Which menu title sits under this column, or -1. */
static int menu_hit(int x)
{
    int m;
    for (m = 0; m < MENU_COUNT; m++) {
        int a = menu_x[m] - 1;
        int b = menu_x[m] + (int)strlen(menu_title[m]) + 1;
        if (x >= a && x <= b)
            return m;
    }
    return -1;
}

/* Drop one menu down and let the user pick. Returns when it closes. */
static void menu_loop(int start)
{
    int m = start, item = 0;

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
        for (i = 0; i < n; i++) {
            int j;
            unsigned char a = (i == item) ? A_MENUSEL : A_MENU;
            for (j = 0; j < w; j++)
                tui_put(x + j, y + 1 + i, ' ', a);
            tui_puts(x + 2, y + 1 + i, items[i], a);
        }
        tui_cursor(-1, -1);
        tui_flush();

        k = tui_getkey();

        /* A click on an item picks it; on a title, switches to that menu;
         * anywhere else closes, which is what a menu is expected to do. */
        if (k == K_MOUSE) {
            int mx, my, mb, hit;
            tui_mouse(&mx, &my, &mb);
            if (my == 0) {
                hit = menu_hit(mx);
                if (hit >= 0) { m = hit; item = 0; continue; }
                return;
            }
            if (mx > x && mx < x + w - 1 && my > y && my <= y + n) {
                item = my - y - 1;
                do_menu_action(m, item);
                if (m == 2) continue;
                return;
            }
            return;
        }

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
        if (k == 10) k = K_ENTER;      /* a terminal that translated it anyway */

        if (k == K_F10) { menu_loop(0); ensure_prompt(); continue; }
        if (k == 17) { quitting = 1; return 0; }            /* Ctrl-Q */
        if (k == 3)  { break_seen = 1; continue; }          /* Ctrl-C */
        if (k == K_F5) { run_line("RUN"); ensure_prompt(); continue; }
        if (k == K_F9) { run_line("LIST"); ensure_prompt(); continue; }
        if (k == K_F3) { do_load(); ensure_prompt(); continue; }
        if (k == K_F2) { do_save(); ensure_prompt(); continue; }
        /* Clicking a pane is the other way to move the keyboard, and
         * clicking a line puts the cursor on it directly -- which is the
         * thing a listing you cannot click at feels wrong for. */
        if (k == K_MOUSE) {
            int mx, my, mb, hit;
            tui_mouse(&mx, &my, &mb);
            status[0] = '\0';
            if (my == 0) {
                hit = menu_hit(mx);
                if (hit >= 0) { menu_loop(hit); ensure_prompt(); }
                continue;
            }
            if (mx >= PROG_X && mx < PROG_X + PROG_W &&
                my >= PROG_Y && my < PROG_Y + PROG_LINES) {
                int row = prog_top + (my - PROG_Y);
                int n = prog_count();
                if (focus == FOCUS_EDIT)
                    ed_commit();
                focus = FOCUS_EDIT;
                ed_row = (row > n) ? n : row;
                ed_load();
                ed_col = mx - PROG_X;
                if (ed_col > (int)strlen(ed_buf))
                    ed_col = (int)strlen(ed_buf);
                ed_scroll_into_view();
                continue;
            }
            if (mx >= APPLE_X && mx < APPLE_X + APPLE_W &&
                my >= APPLE_Y && my < APPLE_Y + APPLE_H) {
                if (focus == FOCUS_EDIT)
                    ed_commit();
                focus = FOCUS_TERM;
                continue;
            }
            continue;
        }

        /* Tab moves the keyboard between the prompt and the listing. Leaving
         * the editor hands whatever was being typed back to the interpreter,
         * so a half-finished edit is not silently lost. */
        if (k == K_TAB) {
            status[0] = '\0';
            if (focus == FOCUS_EDIT) {
                ed_commit();
                focus = FOCUS_TERM;
            } else {
                focus = FOCUS_EDIT;
                if (ed_row > prog_count())
                    ed_row = prog_count();
                ed_col = 0;
                ed_load();
                ed_scroll_into_view();
            }
            continue;
        }

        if (focus == FOCUS_EDIT) {
            int len;
            switch (k) {
            case K_UP:    ed_move(-1); break;
            case K_DOWN:  ed_move(1); break;
            case K_ENTER: ed_move(1); break;
            case K_PGUP:  ed_move(-PROG_LINES); break;
            case K_PGDN:  ed_move(PROG_LINES); break;
            case K_LEFT:  if (ed_col > 0) ed_col--; break;
            case K_HOME:  ed_col = 0; break;
            case K_END:   ed_col = (int)strlen(ed_buf); break;
            case K_BS:    ed_backspace(); break;
            case K_DEL:   ed_del(); break;
            case K_F8:    ed_delete_line(); break;
            case K_RIGHT:
                len = (int)strlen(ed_buf);
                if (ed_col < len) ed_col++;
                break;
            default:
                if (k >= 32 && k < 127)
                    ed_insert(k);
                break;
            }
            continue;
        }

        if (k == K_PGUP) { if (prog_top > 0) prog_top--; continue; }
        if (k == K_PGDN) { prog_top++; continue; }

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

int host_echoes(void)
{
    /* The event loop puts every typed character on the Apple screen already. */
    return 1;
}

int host_pollkey(void)
{
    /* The event loop owns the keyboard, so a poll has to go through it. Only
     * ordinary characters reach a program; function keys stay with the IDE. */
    if (!tui_haskey())
        return 0;
    {
        int k = tui_getkey();
        if (k == 3) { break_seen = 1; return 0; }
        return (k >= 32 && k < 127) ? k : ((k == K_ENTER) ? 13 : 0);
    }
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

/* Just the file's name, not the path it was reached by: the strip is there to
 * say which program is loaded, and a directory tells you nothing about that. */
void ide_set_loaded(const char *name)
{
    const char *base = name ? name : "";
    const char *p;

    for (p = base; *p; p++)
        if (*p == '/' || *p == '\\')
            base = p + 1;
    strncpy(loaded, base, sizeof(loaded) - 1);
    loaded[sizeof(loaded) - 1] = '\0';
}

void ide_start(void)
{
    apple_clear();
    prog_top = 0;
    input_len = 0;
    quitting = 0;

    /* The mockup's Apple pane opens with this, and it is worth having: it
     * says which of the two front ends you are looking at. The console build
     * deliberately prints nothing, because the reference binary prints
     * nothing and every line of its output is compared. */
    scr_puts("APPLESOFT BASIC FOR DOS");
    scr_newline();
    scr_puts("THE ROM'S BUGS, ON PURPOSE");
    scr_newline();
    scr_newline();
}
