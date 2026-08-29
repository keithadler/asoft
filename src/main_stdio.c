/* main_stdio.c - the console front end.
 *
 * A plain REPL: print the Applesoft prompt, read a line, hand it to the
 * interpreter. Everything the program prints goes through the 40-column
 * screen model, so wrapping and POS behave the same here as they do under
 * Turbo Vision; only the destination differs.
 */
#include "bugs.h"
#include "display.h"
#include "gfx.h"
#include "host.h"
#include "interp.h"
#include "screen.h"

#include <stdio.h>
#include <string.h>

static void sink(char ch)
{
    /* While hi-res is up the Apple showed the graphics page, so anything
     * printed went somewhere you could not see. The DOS build honours that;
     * the terminal build does not, because it has only one screen. */
    if (disp_suppress_text())
        return;
    if (ch == '\f') {
        /* HOME. A dumb terminal gets a few blank lines rather than an
         * escape sequence that may not be understood. */
        int i;
        for (i = 0; i < 4; i++)
            putchar('\n');
        return;
    }
    putchar(ch);
}

int host_getline(char *buf, int max)
{
    int len;
    if (!fgets(buf, max, stdin))
        return 0;
    len = (int)strlen(buf);
    while (len && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
        buf[--len] = '\0';
    return 1;
}

int host_getkey(void)
{
    int c = getchar();
    return (c == EOF) ? 0 : c;
}

/* Non-blocking, without disturbing the line-based input the prompt uses: the
 * terminal is put into raw mode for the length of one read and put back. On
 * DOS the runtime already offers exactly this. */
#ifdef __DOS__
#include <conio.h>
int host_pollkey(void)
{
    return kbhit() ? getch() : 0;
}
#else
#include <termios.h>
#include <unistd.h>
int host_pollkey(void)
{
    struct termios old, raw;
    unsigned char c;
    int n;

    if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &old) != 0)
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
#endif

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
            "  -r   run the program straight away, without waiting for RUN\n",
            argv0);
}

int main(int argc, char **argv)
{
    char line[512];
    const char *path = 0;
    int autorun = 0;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0) {
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
    if (autorun)
        it_line("RUN");

    for (;;) {
        /* Repaint before prompting, so a program that drew something is
         * showing it while you decide what to type next. */
        disp_refresh();
        if (!disp_suppress_text()) {
            scr_raw_puts("]");
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
