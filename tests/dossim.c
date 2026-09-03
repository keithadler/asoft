/* dossim - the DOS front end on the host, screen and all.
 *
 * console_dos.c and display_dos.c are compiled here against tests/dosshim,
 * where int 10h is a recorder and video memory is two arrays. A script is
 * typed at the prompt one line at a time, and after any line a snapshot
 * can be taken: the text mode as the characters in B800, the graphics mode
 * as a PPM of A000 through the DAC. So what the 16-bit binary would have
 * put on a real screen is checked here without an emulator, and diffed.
 *
 *   dossim script.txt [snapdir]
 *
 * Script lines: text is typed and entered; "#snap NAME" dumps the screen;
 * "#keys TEXT" queues keys for the program the next line runs to find.
 */
#include "../src/bugs.h"
#include "../src/console_dos.h"
#include "../src/display.h"
#include "../src/gfx.h"
#include "../src/host.h"
#include "../src/interp.h"
#include "../src/pace.h"
#include "../src/screen.h"
#include "dosshim/conio.h"
#include "dosshim/dos.h"

#include <stdio.h>
#include <string.h>

static const char *snapdir = "build";
static int nsnap;

static void snap(const char *name)
{
    int cols = (shim_video_mode == 1) ? 40 : 80;
    int row, col;

    nsnap++;
    printf("--- %s: mode %d", name, shim_video_mode);
    if (shim_video_mode == 0x13) {
        char path[256];
        FILE *f;
        int i;
        sprintf(path, "%s/dossim-%s.ppm", snapdir, name);
        f = fopen(path, "wb");
        if (f) {
            fprintf(f, "P6\n320 200\n255\n");
            for (i = 0; i < 64000; i++)
                fwrite(shim_dac[shim_vga[i]], 1, 3, f);
            fclose(f);
        }
        printf(" (%s)\n", path);
        return;
    }
    printf(", %d columns, cursor %d,%d\n", cols, shim_cursor_row, shim_cursor_col);
    for (row = 0; row < 24; row++) {
        char line[84];
        for (col = 0; col < cols; col++) {
            unsigned char ch = shim_text[(row * cols + col) * 2];
            unsigned char at = shim_text[(row * cols + col) * 2 + 1];
            if (ch < 32 || ch > 126) ch = '?';
            /* Inverse and flashing cells are marked, since the glyph alone
             * cannot show them. */
            if (at == 0x70) ch = (char)((ch == ' ') ? '#' : (ch | 0x80));
            else if (at == 0x87) ch = '~';
            line[col] = (char)ch;
        }
        line[cols] = '\0';
        for (col = cols - 1; col >= 0 && line[col] == ' '; col--)
            line[col] = '\0';
        for (col = 0; line[col]; col++)
            if ((unsigned char)line[col] & 0x80) putchar((line[col] & 0x7F) | 0x20); /* inverse letter: lower case */
            else putchar(line[col]);
        putchar('\n');
    }
}

/* "#waitsnap NAME KEYS": when the program next blocks on the keyboard,
 * snapshot the screen it is showing as NAME, then type KEYS. */
static char waitsnap_name[64], waitsnap_keys[64];
static void waitsnap(void)
{
    snap(waitsnap_name);
    shim_type(waitsnap_keys);
}

int main(int argc, char **argv)
{
    char line[512];
    char deferred[64] = "";
    FILE *script;

    if (argc < 2) { fprintf(stderr, "usage: dossim script [snapdir]\n"); return 2; }
    if (argc > 2) snapdir = argv[2];
    script = fopen(argv[1], "r");
    if (!script) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }

    pace_set_rate(0);
    con_init();
    it_init();
    disp_init();
    gfx_on_change(disp_touch);
    con_start();

    while (fgets(line, (int)sizeof(line), script)) {
        char typed[512];
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = '\0';
        if (strncmp(line, "#snap ", 6) == 0) { snap(line + 6); continue; }
        if (strncmp(line, "#keys ", 6) == 0) { strcpy(deferred, line + 6); continue; }
        if (strncmp(line, "#waitsnap ", 10) == 0) {
            char *sp = strchr(line + 10, ' ');
            if (sp) { *sp = '\0'; strcpy(waitsnap_keys, sp + 1); } else waitsnap_keys[0] = '\0';
            strcpy(waitsnap_name, line + 10);
            shim_on_empty(waitsnap);
            continue;
        }
        if (line[0] == '#') continue;

        scr_raw_putc(scr_prompt());
        strcpy(typed, line);
        strcat(typed, "\r");
        shim_type(typed);
        if (!host_getline(line, (int)sizeof(line)))
            break;
        /* Keys for the program the line is about to run, not for the line. */
        shim_type(deferred);
        deferred[0] = '\0';
        it_line(line);
    }
    disp_shutdown();
    printf("--- end: mode %d, %d mode sets, %d snapshots\n",
           shim_video_mode, shim_mode_sets, nsnap);
    return 0;
}
