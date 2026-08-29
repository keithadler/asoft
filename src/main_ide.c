/* Entry point for the windowed front end.
 *
 * The REPL is the same shape as the console build's: print a prompt, read a
 * line, hand it to the interpreter. Everything windowed happens inside
 * host_getline, which is where ide.c runs its event loop -- so the
 * interpreter itself does not know which front end it is under.
 */
#include "bugs.h"
#include "host.h"
#include "interp.h"
#include "screen.h"
#include "tui.h"

#include <stdio.h>
#include <string.h>

void ide_sink(char ch);
void ide_start(void);
int  ide_quitting(void);

static void usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s [-n] [program.bas]\n"
            "  -n   disable the deliberate ROM bugs\n", argv0);
}

int main(int argc, char **argv)
{
    char line[256];
    const char *path = 0;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0) {
            memset(bug_enabled, 0, sizeof(bug_enabled));
        } else if (argv[i][0] == '-' && argv[i][1]) {
            usage(argv[0]);
            return 2;
        } else {
            path = argv[i];
        }
    }

    scr_init(ide_sink);
    it_init();
    ide_start();
    tui_init();

    if (path && !it_load(path)) {
        tui_shutdown();
        fprintf(stderr, "cannot open %s\n", path);
        return 1;
    }

    for (;;) {
        scr_raw_puts("]");
        if (!host_getline(line, (int)sizeof(line)))
            break;
        it_line(line);
        if (it_quitting() || ide_quitting())
            break;
    }

    tui_shutdown();
    return 0;
}
