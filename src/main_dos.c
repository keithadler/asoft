/* main_dos.c - the DOS front end.
 *
 * The same REPL as the console build everywhere else: print the Applesoft
 * prompt, read a line, hand it to the interpreter. What differs is where
 * the characters go. Here there is a real screen, and console_dos.c and
 * display_dos.c between them make it the Apple's: a text page at $400 in
 * a forty- or eighty-column text mode, graphics pages in mode 13h, and
 * every switch between them a mode set. Redirected, it is a stream again.
 */
#include "bugs.h"
#include "console_dos.h"
#include "display.h"
#include "gfx.h"
#include "host.h"
#include "interp.h"
#include "pace.h"
#include "screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    con_init();
    it_init();
    disp_init();
    gfx_on_change(disp_touch);
    con_start();

    if (path && !it_load(path)) {
        disp_shutdown();
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
        disp_refresh();
        /* The prompt goes to the page whether or not the page is on show:
         * with HGR2 up you type blind, exactly as you did. A stream has no
         * page, so there it is held back instead. */
        if (con_interactive() || !disp_suppress_text()) {
            scr_raw_putc(scr_prompt());
            if (!con_interactive())
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
