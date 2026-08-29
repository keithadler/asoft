/* Terminal backend: the cell buffer as ANSI.
 *
 * Redraws only the rows that changed, because pushing 3440 cells through a
 * terminal on every keystroke is visibly slow over anything but a local pty.
 */
#include "tui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

static struct {
    unsigned char ch[TUI_H][TUI_W];
    unsigned char at[TUI_H][TUI_W];
} buf, shown;

static int cur_x = -1, cur_y = -1;
static int started;
static struct termios saved_term;

/* CP437 box drawing has no ASCII equivalent worth using, so spell the glyphs
 * with the Unicode characters they were drawn from. */
static const char *glyph(int ch)
{
    switch (ch) {
    case G_TL:    return "\xe2\x95\x94";   /* double corners */
    case G_TR:    return "\xe2\x95\x97";
    case G_BL:    return "\xe2\x95\x9a";
    case G_BR:    return "\xe2\x95\x9d";
    case G_HORZ:  return "\xe2\x95\x90";
    case G_VERT:  return "\xe2\x95\x91";
    case G_SHADE: return "\xe2\x96\x91";
    case G_RULE:  return "\xe2\x94\x80";
    case G_HALF:  return "\xe2\x96\x80";
    default:      return 0;
    }
}

/* Truecolour, so the terminal shows the same palette the DAC is loaded with
 * on DOS rather than whatever sixteen colours the user's theme happens to
 * define. */
static void put_colour(int fg, int bg)
{
    const unsigned char *f = tui_palette[fg & 15];
    const unsigned char *b = tui_palette[bg & 15];
    printf("\033[38;2;%d;%d;%d;48;2;%d;%d;%dm",
           f[0], f[1], f[2], b[0], b[1], b[2]);
}

void tui_init(void)
{
    struct termios t;

    if (tcgetattr(STDIN_FILENO, &saved_term) == 0) {
        t = saved_term;
        t.c_lflag &= (unsigned long)~(ICANON | ECHO);
        /* Without this the line discipline turns a carriage return into a
         * newline before it ever reaches us, so Return arrives as 10 and the
         * key never matches. */
        t.c_iflag &= (unsigned long)~(ICRNL | INLCR | IGNCR);
        t.c_cc[VMIN] = 1;
        t.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &t);
        started = 1;
    }
    printf("\033[?1049h");            /* alternate screen, so the shell survives */
    printf("\033[?25l");
    /* Report button presses, in the SGR encoding: it is the only one that
     * copes with a screen wider than 95 columns, which 80 is not, but it also
     * distinguishes press from release, which the old encoding does not. */
    printf("\033[?1000h\033[?1006h");
    memset(&shown, 0xFF, sizeof(shown));
    fflush(stdout);
}

void tui_shutdown(void)
{
    printf("\033[?1006l\033[?1000l");
    printf("\033[0m\033[?25h\033[?1049l");
    fflush(stdout);
    if (started)
        tcsetattr(STDIN_FILENO, TCSANOW, &saved_term);
}

void tui_clear(unsigned char attr)
{
    int y, x;
    for (y = 0; y < TUI_H; y++)
        for (x = 0; x < TUI_W; x++) {
            buf.ch[y][x] = ' ';
            buf.at[y][x] = attr;
        }
}

void tui_put(int x, int y, int ch, unsigned char attr)
{
    if (x < 0 || x >= TUI_W || y < 0 || y >= TUI_H)
        return;
    buf.ch[y][x] = (unsigned char)ch;
    buf.at[y][x] = attr;
}

void tui_puts(int x, int y, const char *s, unsigned char attr)
{
    while (*s && x < TUI_W)
        tui_put(x++, y, (unsigned char)*s++, attr);
}

void tui_fill(int x, int y, int w, int h, int ch, unsigned char attr)
{
    int i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            tui_put(x + i, y + j, ch, attr);
}

void tui_box(int x, int y, int w, int h, const char *title, unsigned char attr)
{
    int i, len, tx;

    tui_put(x, y, G_TL, attr);
    tui_put(x + w - 1, y, G_TR, attr);
    tui_put(x, y + h - 1, G_BL, attr);
    tui_put(x + w - 1, y + h - 1, G_BR, attr);
    for (i = 1; i < w - 1; i++) {
        tui_put(x + i, y, G_HORZ, attr);
        tui_put(x + i, y + h - 1, G_HORZ, attr);
    }
    for (i = 1; i < h - 1; i++) {
        tui_put(x, y + i, G_VERT, attr);
        tui_put(x + w - 1, y + i, G_VERT, attr);
    }
    if (title && *title) {
        len = (int)strlen(title);
        tx = x + (w - len - 2) / 2;
        tui_put(tx, y, ' ', attr);
        tui_puts(tx + 1, y, title, attr);
        tui_put(tx + len + 1, y, ' ', attr);
    }
}

int tui_cell(int x, int y)
{
    if (x < 0 || x >= TUI_W || y < 0 || y >= TUI_H)
        return ' ';
    return buf.ch[y][x];
}

unsigned char tui_attr(int x, int y)
{
    if (x < 0 || x >= TUI_W || y < 0 || y >= TUI_H)
        return 0;
    return buf.at[y][x];
}

void tui_cursor(int x, int y) { cur_x = x; cur_y = y; }

void tui_flush(void)
{
    int y, x, last = -1;

    for (y = 0; y < TUI_H; y++) {
        if (memcmp(buf.ch[y], shown.ch[y], TUI_W) == 0 &&
            memcmp(buf.at[y], shown.at[y], TUI_W) == 0)
            continue;
        printf("\033[%d;1H", y + 1);
        last = -1;
        for (x = 0; x < TUI_W; x++) {
            unsigned char a = buf.at[y][x];
            const char *g;
            if (a != last) {
                put_colour(a & 15, a >> 4);
                last = a;
            }
            g = glyph(buf.ch[y][x]);
            if (g)
                fputs(g, stdout);
            else
                putchar(buf.ch[y][x] < 32 ? ' ' : buf.ch[y][x]);
        }
        memcpy(shown.ch[y], buf.ch[y], TUI_W);
        memcpy(shown.at[y], buf.at[y], TUI_W);
    }
    if (cur_x >= 0 && cur_y >= 0) {
        printf("\033[%d;%dH\033[?25h", cur_y + 1, cur_x + 1);
    } else {
        printf("\033[?25l");
    }
    fflush(stdout);
}

/* --- keys --------------------------------------------------------------- */

static int pending = -1;
static int mouse_x, mouse_y, mouse_b;

void tui_mouse(int *x, int *y, int *button)
{
    *x = mouse_x; *y = mouse_y; *button = mouse_b;
}

static int raw_getc(void)
{
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) != 1)
        return -1;
    return c;
}

/* Is there input waiting? Asking the terminal driver costs one syscall;
 * changing the terminal mode to find out costs three, and a program polling
 * the keyboard in a tight loop -- which is what a game does -- pays that on
 * every single poll. */
static int input_ready(long usec)
{
    fd_set r;
    struct timeval tv;

    FD_ZERO(&r);
    FD_SET(STDIN_FILENO, &r);
    tv.tv_sec = usec / 1000000L;
    tv.tv_usec = usec % 1000000L;
    return select(STDIN_FILENO + 1, &r, 0, 0, &tv) > 0;
}

int tui_haskey(void)
{
    return (pending >= 0) || input_ready(0);
}

/* Wait briefly for the next byte. An escape sequence does not always arrive
 * in one piece -- over a pty, a pipe, or a slow link the terminal can hand it
 * over a byte at a time -- and giving up immediately is the difference
 * between reading F5 and reading Escape followed by "[15~" as ordinary text.
 * A bare Escape still comes back promptly, because nothing follows it. */
static int wait_for_byte(long usec)
{
    int c;

    if (pending >= 0)
        return 1;
    if (!input_ready(usec))
        return 0;
    c = raw_getc();
    if (c < 0)
        return 0;
    pending = c;
    return 1;
}

/* Decode the escape sequences for the keys the IDE actually binds. Anything
 * unrecognised comes back as ESC, which is what a bare Escape sends anyway. */
static int decode_escape(void)
{
    int a, b;

    if (!wait_for_byte(100000L))
        return K_ESC;
    a = pending; pending = -1;
    if (a != '[' && a != 'O')
        return K_ESC;
    if (!wait_for_byte(100000L))
        return K_ESC;
    b = pending; pending = -1;

    /* ESC [ < button ; col ; row M for a press, m for a release. */
    if (b == '<') {
        int n[3], i = 0, v = 0, c;
        n[0] = n[1] = n[2] = 0;
        for (;;) {
            c = raw_getc();
            if (c >= '0' && c <= '9') { v = v * 10 + (c - '0'); continue; }
            if (i < 3) n[i++] = v;
            v = 0;
            if (c == ';') continue;
            break;                      /* M, m, or something malformed */
        }
        if (c != 'M' || n[0] > 2)
            return K_ESC;               /* a release, or a drag: ignore it */
        mouse_b = n[0];
        mouse_x = n[1] - 1;             /* the wire format counts from one */
        mouse_y = n[2] - 1;
        return K_MOUSE;
    }

    switch (b) {
    case 'A': return K_UP;
    case 'B': return K_DOWN;
    case 'C': return K_RIGHT;
    case 'D': return K_LEFT;
    case 'H': return K_HOME;
    case 'F': return K_END;
    case 'P': return K_F1;
    case 'Q': return K_F2;
    case 'R': return K_F3;
    case 'S': return K_F4;
    default: break;
    }
    if (b >= '0' && b <= '9') {
        int n = b - '0', c;
        while ((c = raw_getc()) >= '0' && c <= '9')
            n = n * 10 + (c - '0');
        /* c is the terminator, '~' for most of these. */
        switch (n) {
        case 1: case 7: return K_HOME;
        case 4: case 8: return K_END;
        case 3:  return K_DEL;
        case 5:  return K_PGUP;
        case 6:  return K_PGDN;
        case 11: return K_F1;
        case 12: return K_F2;
        case 13: return K_F3;
        case 14: return K_F4;
        case 15: return K_F5;
        case 17: return K_F6;
        case 18: return K_F7;
        case 19: return K_F8;
        case 20: return K_F9;
        case 21: return K_F10;
        default: return K_ESC;
        }
    }
    return K_ESC;
}

int tui_getkey(void)
{
    int c;

    if (pending >= 0) { c = pending; pending = -1; }
    else c = raw_getc();

    if (c < 0)
        return K_ESC;
    if (c == 27)
        return decode_escape();
    if (c == 127)
        return K_BS;
    return c;
}
