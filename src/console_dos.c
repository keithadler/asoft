/* console_dos.c - the DOS console: the screen model's sink, and the keyboard.
 *
 * On the machine there was no stream. PRINT stored bytes into the text page
 * at the cursor, the ROM's GETLN echoed what you typed into the same page,
 * and the video hardware showed whatever the page and the switches said.
 * This is that: characters go to the page through the video layer, the
 * cursor is the screen model's cursor, and the line editor is GETLN's --
 * echo, backspace, Ctrl-X to throw the line away, return to hand it over.
 *
 * When the build is run with its input or output redirected there is no
 * console to be, and it falls back to being the stream it was, so the
 * capture rig's transcripts still come out the same.
 */
#include "console_dos.h"
#include "display.h"
#include "gfx.h"
#include "host.h"
#include "screen.h"
#include "sound.h"
#include "video_dos.h"

#include <conio.h>
#include <io.h>
#include <stdio.h>
#include <string.h>

static int interactive;
static int crow, ccol;               /* where the next character lands */

/* --- the page --------------------------------------------------------- */

/* The window's edges, as the ROM's zero page has them right now. */
#define WTOP    scr_window_top()
#define WBOT    scr_window_bottom()
#define WLEFT   scr_window_left()
#define WRIGHT  (scr_window_left() + scr_window_width())   /* one past */

/* Down a line, scrolling the text window when the cursor is already on
 * its bottom one. */
static void con_newline(void)
{
    ccol = WLEFT;
    if (crow + 1 < WBOT)
        crow++;
    else
        vid_text_scroll(WTOP, WBOT, WLEFT, WRIGHT - WLEFT);
}

/* This keeps its own cursor rather than asking the screen model for one.
 * The model's column is what POS reports, and raw output -- the "]" prompt,
 * and what you type after it -- deliberately does not advance it. On the
 * real display every character takes a cell whether or not it counts, so
 * the console counts them itself and resyncs whenever the model says the
 * cursor moved. */
static void con_sink(char ch)
{
    if (ch == SCR_CLEAR) {
        /* A clear: of the whole screen when the card changed (PR#), of
         * the text window otherwise (HOME). */
        if (scr_card_cols() != vid_cols())
            vid_text_cols(scr_card_cols());
        else
            vid_text_clear(WTOP, WBOT, WLEFT, WRIGHT - WLEFT);
        crow = scr_row();
        ccol = WLEFT + scr_col();
        return;
    }
    if (ch == SCR_CLREOL) {
        vid_text_fill(crow, ccol, WRIGHT - 1);
        return;
    }
    if (ch == SCR_CLREOP) {
        int r;
        vid_text_fill(crow, ccol, WRIGHT - 1);
        for (r = crow + 1; r < WBOT; r++)
            vid_text_fill(r, WLEFT, WRIGHT - 1);
        return;
    }
    if (ch == '\n') {
        con_newline();
        return;
    }
    if (ccol >= WRIGHT)
        con_newline();
    vid_text_put(crow, ccol, scr_encode(ch));
    ccol++;
}

/* The screen model moved the cursor without printing: HTAB, VTAB, HOME,
 * a POKE into 36 or 37, or TEXT and GR parking it on the bottom line.
 * Follow it. The model's column is within the window; the page's is not. */
static void con_cursor_moved(void)
{
    crow = scr_row();
    ccol = WLEFT + scr_col();
}

/* --- the stream fallback ------------------------------------------------ */

/* The sink when there is no console: bytes to stdout, a clear as a few
 * blank lines, and nothing at all while a full-screen picture is up. */
static void stream_sink(char ch)
{
    if (disp_suppress_text())
        return;
    if (ch == SCR_CLEAR) {
        int i;
        for (i = 0; i < 4; i++)
            putchar('\n');
        return;
    }
    if (ch == SCR_CLREOL || ch == SCR_CLREOP)
        return;                       /* nothing a stream can clear */
    putchar(ch);
}

/* --- keys --------------------------------------------------------------- */

/* One key from the BIOS, with extended keys (arrows, function keys) folded
 * to nothing: they arrive as a zero byte and a scan code, and the Apple had
 * no such keys to give a program. */
static int read_key(void)
{
    int c = getch();
    if (c == 0 || c == 0xE0) {
        (void)getch();
        return 0;
    }
    return c;
}

/* Rub out the character before the cursor, wrapping back up a line if the
 * typed line had wrapped. The Apple's left arrow was non-destructive; this
 * is the PC's backspace, because that is the key under the finger. */
static void backspace(void)
{
    if (ccol > WLEFT) {
        ccol--;
    } else if (crow > WTOP) {
        crow--;
        ccol = WRIGHT - 1;
    } else {
        return;
    }
    vid_text_put(crow, ccol, TEXT_BLANK);
}

/* The ROM's GETLN, more or less: echo what is typed, take return as the
 * end, and offer Ctrl-X to throw the line away. Redirected, read the file. */
int host_getline(char *buf, int max)
{
    int len = 0;

    if (!interactive) {
        int n, i, j;
        if (!fgets(buf, max, stdin))
            return 0;
        n = (int)strlen(buf);
        while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
            buf[--n] = '\0';
        /* The ROM's GETLN never let a control character into the line. */
        for (i = j = 0; i < n; i++)
            if ((unsigned char)buf[i] >= 32)
                buf[j++] = buf[i];
        buf[j] = '\0';
        return 1;
    }

    for (;;) {
        int c;
        vid_text_cursor(crow, ccol);
        c = read_key();
        if (c == 0)
            continue;
        if (c == '\r' || c == '\n')
            break;
        if (c == 8 || c == 127) {
            if (len > 0) {
                len--;
                backspace();
            }
            continue;
        }
        if (c == 3) {                 /* Ctrl-C: nothing to run */
            len = 0;
            break;
        }
        if (c == 24) {                /* Ctrl-X: the ROM printed \ and started over */
            con_sink('\\');
            len = 0;
            break;
        }
        if (c == 7) {                 /* Ctrl-G, the bell, typed */
            snd_bell();
            continue;
        }
        if (c < 32)
            continue;
        if (len < max - 1) {
            buf[len++] = (char)c;
            con_sink((char)c);
        }
    }
    buf[len] = '\0';
    vid_text_cursor(-1, -1);
    scr_newline();                    /* GETLN echoed the return too */
    return 1;
}

/* GET: one key, waited for, not echoed. */
int host_getkey(void)
{
    if (!interactive) {
        int c = getchar();
        return (c == EOF) ? 0 : c;
    }
    for (;;) {
        int c = read_key();
        if (c)
            return c;
    }
}

/* The keyboard strobe: a key if one is waiting, without waiting. */
int host_pollkey(void)
{
    if (!kbhit())
        return 0;
    return read_key();
}

int host_echoes(void)
{
    /* Typing at the console is echoed by the line editor above; a file
     * being piped in is not, and INPUT then has to echo the answer itself
     * or the transcript records the question and not the reply. */
    return interactive;
}

int host_break(void)
{
    /* Poll the keyboard between statements: a pending Ctrl-C stops the
     * program, the way the ROM's GETLN check did. Anything else is pushed
     * back for the program's own polling (SNAKE reads -16384). Every eighth
     * statement is often enough; the BIOS call is not free. */
    static unsigned nth;
    if ((++nth & 7) != 0)
        return 0;
    if (kbhit()) {
        int c = getch();
        if (c == 3)
            return 1;
        ungetch(c);
    }
    return 0;
}

/* --- setup -------------------------------------------------------------- */

/* Decide whether there is a console to be, and register the sink: the
 * page-writing one when stdin and stdout are the screen and keyboard, the
 * stream one when either is a file. */
void con_init(void)
{
    interactive = isatty(fileno(stdin)) && isatty(fileno(stdout));
    if (!interactive) {
        scr_init(stream_sink);
        return;
    }
    scr_init(con_sink);
    scr_on_cursor(con_cursor_moved);
}

/* Switch the forty-column screen on, cleared, once the memory image the
 * page lives in has been set up. */
void con_start(void)
{
    if (!interactive)
        return;
    vid_text_cols(SCR_COLS);          /* the forty-column screen, cleared */
    crow = ccol = 0;
}

int con_interactive(void) { return interactive; }
