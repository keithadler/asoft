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
void ide_draw_once(void);
int  ide_quitting(void);

/* Write the drawn screen out as plain text and stop. There is no pty inside
 * DOSBox to read an escape stream back from, so this is how the DOS build's
 * layout gets checked: run it, read the file out of the virtual disk, diff. */
static int dump_screen(const char *path)
{
    FILE *f = fopen(path, "w");
    int x, y;

    if (!f)
        return 0;
    for (y = 0; y < TUI_H; y++) {
        for (x = 0; x < TUI_W; x++) {
            int c = tui_cell(x, y);
            switch (c) {
            case G_TL: case G_TR: case G_BL: case G_BR: c = '+'; break;
            case G_HORZ: c = '='; break;
            case G_VERT: c = '|'; break;
            case G_SHADE: c = ':'; break;
            case G_RULE: c = '-'; break;
            default: if (c < 32) c = ' '; break;
            }
            fputc(c, f);
        }
        fputc('\n', f);
    }
    fclose(f);
    return 1;
}

static void usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s [-n] [-dump FILE] [program.bas]\n"
            "  -n         disable the deliberate ROM bugs\n"
            "  -dump F    draw the screen once, write it to F as text, exit\n",
            argv0);
}

int main(int argc, char **argv)
{
    char line[256];
    const char *path = 0;
    const char *dump = 0;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-dump") == 0 && i + 1 < argc) {
            dump = argv[++i];
        } else if (strcmp(argv[i], "-n") == 0) {
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

    if (dump) {
        scr_raw_puts("]");
        ide_draw_once();
        tui_shutdown();
        return dump_screen(dump) ? 0 : 1;
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
