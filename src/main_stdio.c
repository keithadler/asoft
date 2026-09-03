/* main_stdio.c - the console front end for the host build.
 *
 * A plain REPL: print the Applesoft prompt, read a line, hand it to the
 * interpreter. Everything the program prints goes through the 40-column
 * screen model, so wrapping and POS behave the same here as they do under
 * the windowed front end; only the destination differs. The DOS build has
 * its own front end (main_dos.c), because there it has a real screen.
 */
#include "bugs.h"
#include "display.h"
#include "gfx.h"
#include "host.h"
#include "interp.h"
#include "pace.h"
#include "screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sink(char ch)
{
    if (disp_suppress_text())
        return;
    if (ch == SCR_CLEAR) {
        /* HOME. A dumb terminal gets a few blank lines rather than an
         * escape sequence that may not be understood. */
        int i;
        for (i = 0; i < 4; i++)
            putchar('\n');
        return;
    }
    if (ch == SCR_CLREOL || ch == SCR_CLREOP)
        return;                       /* nothing a stream can clear */
    putchar(ch);
}

int host_getline(char *buf, int max)
{
    int len, i, j;
    if (!fgets(buf, max, stdin))
        return 0;
    len = (int)strlen(buf);
    while (len && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
        buf[--len] = '\0';
    /* The ROM's GETLN never let a control character into the line; a Ctrl-C
     * pressed at the prompt should not turn into a token, so drop them. */
    for (i = j = 0; i < len; i++)
        if ((unsigned char)buf[i] >= 32)
            buf[j++] = buf[i];
    buf[j] = '\0';
    return 1;
}

int host_getkey(void)
{
    int c = getchar();
    return (c == EOF) ? 0 : c;
}

/* Non-blocking, without disturbing the line-based input the prompt uses: the
 * terminal is put into raw mode for the length of one read and put back. */
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
int host_pollkey(void)
{
    struct termios old, raw;
    unsigned char c;
    fd_set r;
    struct timeval tv;
    int n;

    if (!isatty(STDIN_FILENO))
        return 0;
    /* Ask first, and only touch the terminal mode when there is something to
     * read. A program polling the keyboard does so in a tight loop, and
     * changing modes twice per poll to find out there was nothing is most of
     * the cost of the loop. */
    FD_ZERO(&r);
    FD_SET(STDIN_FILENO, &r);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    if (select(STDIN_FILENO + 1, &r, 0, 0, &tv) <= 0)
        return 0;
    if (tcgetattr(STDIN_FILENO, &old) != 0)
        return 0;
    raw = old;
    raw.c_lflag &= (unsigned long)~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    n = (int)read(STDIN_FILENO, &c, 1);
    tcsetattr(STDIN_FILENO, TCSANOW, &old);
    return (n == 1) ? c : 0;
}

int host_echoes(void)
{
    return isatty(STDIN_FILENO);
}

int host_break(void)
{
    /* Nothing to poll: a redirected stdin cannot deliver Ctrl-C, and an
     * interactive one lets the signal handler deal with it. */
    return 0;
}

static void usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s [-n] [-r] [program.bas]\n"
            "  -n   disable the deliberate ROM bugs\n"
            "  -r   run the program straight away, without waiting for RUN\n"
            "  -f   run flat out, even when a program polls the keyboard\n"
            "  -p   run at the machine's speed from the first statement, not\n"
            "       only once a program polls the keyboard\n"
            "  -s N the machine's speed is N statements a second (0 is -f)\n"
            "  -b   benchmark: run the program flat out and report the rate\n",
            argv0);
}

int main(int argc, char **argv)
{
    char line[512];
    const char *path = 0;
    int autorun = 0;
    int bench = 0;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0) {
            pace_set_rate(0);
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            pace_set_rate(atol(argv[++i]));
        } else if (strcmp(argv[i], "-b") == 0) {
            bench = 1;
            autorun = 1;
            pace_set_rate(0);
        } else if (strcmp(argv[i], "-p") == 0) {
            pace_set_always(1);
        } else if (strcmp(argv[i], "-r") == 0) {
            autorun = 1;
        } else if (strcmp(argv[i], "-n") == 0) {
            int b;
            for (b = 0; b < BUG_COUNT; b++)
                bug_enabled[b] = 0;
        } else if (argv[i][0] == '-') {
            usage(argv[0]);
            return 2;
        } else {
            path = argv[i];
        }
    }

    scr_init(sink);
    it_init();
    disp_init();
    gfx_on_change(disp_touch);

    if (path && !it_load(path)) {
        fprintf(stderr, "cannot open %s\n", path);
        return 1;
    }
    if (autorun) {
        long t0 = pace_now_us(), s0 = pace_total();
        it_line("RUN");
        if (bench) {
            /* Statements a second, from the wall clock: the one number that
             * says whether a machine can hold the Apple's pace. Printed raw,
             * so the forty-column wrap does not cut the number in half. */
            char buf[80];
            long us = pace_now_us() - t0, n = pace_total() - s0;
            double rate;
            if (us < 1) us = 1;
            rate = (double)n * 1000000.0 / (double)us;
            sprintf(buf, "%ld STATEMENTS IN %ld.%03ld S: %ld A SECOND",
                    n, us / 1000000L, (us % 1000000L) / 1000, (long)rate);
            /* After the screen is handed back, so the report is not wiped
             * by the mode change on DOS and lands on stdout everywhere. */
            disp_shutdown();
            printf("\n%s\n", buf);
            fflush(stdout);
            return 0;
        }
    }

    for (;;) {
        /* Repaint before prompting, so a program that drew something is
         * showing it while you decide what to type next. */
        disp_refresh();
        if (!disp_suppress_text()) {
            scr_raw_putc(scr_prompt());
            fflush(stdout);
        }
        if (!host_getline(line, (int)sizeof(line)))
            break;
        it_line(line);
        if (it_quitting())
            break;
    }
    disp_shutdown();
    return 0;
}
