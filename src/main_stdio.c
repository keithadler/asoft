/* main_stdio.c - the console front end.
 *
 * A plain REPL: print the Applesoft prompt, read a line, hand it to the
 * interpreter. Everything the program prints goes through the 40-column
 * screen model, so wrapping and POS behave the same here as they do under
 * Turbo Vision; only the destination differs.
 */
#include "bugs.h"
#include "host.h"
#include "interp.h"
#include "screen.h"

#include <stdio.h>
#include <string.h>

static void sink(char ch)
{
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

int host_break(void)
{
    /* Nothing to poll: a redirected stdin cannot deliver Ctrl-C, and an
     * interactive one lets the signal handler deal with it. */
    return 0;
}

static void usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s [-n] [program.bas]\n"
            "  -n   disable the deliberate ROM bugs\n", argv0);
}

int main(int argc, char **argv)
{
    char line[512];
    const char *path = 0;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0) {
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

    if (path && !it_load(path)) {
        fprintf(stderr, "cannot open %s\n", path);
        return 1;
    }

    for (;;) {
        scr_raw_puts("]");
        fflush(stdout);
        if (!host_getline(line, (int)sizeof(line)))
            break;
        it_line(line);
        if (it_quitting())
            break;
    }
    return 0;
}
