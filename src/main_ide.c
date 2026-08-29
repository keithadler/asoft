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
#include <stdlib.h>

#include <stdio.h>
#include <string.h>

void ide_sink(char ch);
void ide_start(void);
void ide_draw_once(void);
void ide_set_loaded(const char *name);
void ide_set_screenfile(const char *path, void (*writer)(const char *));
int  ide_quitting(void);

/* The same screen as HTML, colours and all, for looking at a design without
 * needing the terminal it was drawn for. */
static int dump_html(const char *path)
{
    static const char *ent[] = { "&", "&amp;", "<", "&lt;", ">", "&gt;", 0 };
    FILE *f = fopen(path, "w");
    int x, y, i;

    if (!f)
        return 0;
    fprintf(f, "<!doctype html><meta charset=utf-8><title>asoft</title>\n");
    fprintf(f, "<style>body{background:#0d0d12;margin:0;padding:28px;"
               "display:flex;justify-content:center}"
               "pre{font:14px/1.15 'SF Mono',Menlo,monospace;margin:0;"
               "border-radius:6px;overflow:hidden;padding:10px 12px;"
               "background:rgb(%d,%d,%d)}"
               "span{white-space:pre}</style>\n<pre>",
            tui_palette[0][0], tui_palette[0][1], tui_palette[0][2]);
    for (y = 0; y < TUI_H; y++) {
        for (x = 0; x < TUI_W; x++) {
            unsigned char a = tui_attr(x, y);
            const unsigned char *fg = tui_palette[a & 15];
            const unsigned char *bg = tui_palette[a >> 4];
            int c = tui_cell(x, y);
            const char *rep = 0;

            switch (c) {
            case G_TL: case G_TR: case G_BL: case G_BR: c = '+'; break;
            case G_HORZ: rep = "\xe2\x95\x90"; break;
            case G_VERT: rep = "\xe2\x95\x91"; break;
            case G_SHADE: rep = "\xe2\x96\x91"; break;
            case G_RULE: rep = "\xe2\x94\x80"; break;
            default: if (c < 32) c = ' '; break;
            }
            fprintf(f, "<span style=\"color:rgb(%d,%d,%d);background:rgb(%d,%d,%d)\">",
                    fg[0], fg[1], fg[2], bg[0], bg[1], bg[2]);
            if (rep) {
                fputs(rep, f);
            } else {
                for (i = 0; ent[i]; i += 2)
                    if (c == ent[i][0]) break;
                if (ent[i]) fputs(ent[i + 1], f);
                else fputc(c, f);
            }
            fputs("</span>", f);
        }
        fputc('\n', f);
    }
    fprintf(f, "</pre>\n");
    fclose(f);
    return 1;
}

/* Write the drawn screen out as plain text and stop. There is no pty inside
 * DOSBox to read an escape stream back from, so this is how the DOS build's
 * layout gets checked: run it, read the file out of the virtual disk, diff. */
static int dump_screen(const char *path);

/* The same, as a void, for ide.c to call after each redraw. */
static void write_screen(const char *path)
{
    (void)dump_screen(path);
}

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
            "  -dump F    draw the screen once, write it to F as text, exit\n"
            "  -screen F  keep running, rewriting F after every redraw\n",
            argv0);
}

int main(int argc, char **argv)
{
    char line[256];
    const char *path = 0;
    const char *dump = 0;
    const char *screen = 0;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-screen") == 0 && i + 1 < argc) {
            screen = argv[++i];
        } else if (strcmp(argv[i], "-dump") == 0 && i + 1 < argc) {
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

    if (screen)
        ide_set_screenfile(screen, write_screen);
    if (path)
        ide_set_loaded(path);
    if (path && !it_load(path)) {
        tui_shutdown();
        fprintf(stderr, "cannot open %s\n", path);
        return 1;
    }

    if (dump) {
        scr_raw_puts("]");
        ide_draw_once();
        tui_shutdown();
        {
            size_t n = strlen(dump);
            int ok = (n > 5 && strcmp(dump + n - 5, ".html") == 0)
                   ? dump_html(dump) : dump_screen(dump);
            return ok ? 0 : 1;
        }
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
