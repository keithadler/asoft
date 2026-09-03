#include "dos.h"
#include "conio.h"
#include <stdio.h>
#include <string.h>

int shim_video_mode = 3;
int shim_mode_sets;
int shim_cursor_row = -1, shim_cursor_col = -1;
unsigned char shim_vga[64000];
unsigned char shim_text[8000];
unsigned char shim_dac[256][3];
static int dac_index, dac_chan;

void *shim_mem(unsigned seg)
{
    if (seg == 0xA000) return shim_vga;
    if (seg == 0xB800) return shim_text;
    return 0;
}

int int86(int intno, union REGS *in, union REGS *out)
{
    if (intno == 0x10) {
        if (in->h.ah == 0) {
            shim_video_mode = in->h.al;
            shim_mode_sets++;
            /* A mode set clears the screen, as the BIOS does. */
            memset(shim_vga, 0, sizeof(shim_vga));
            memset(shim_text, 0, sizeof(shim_text));
        } else if (in->h.ah == 2) {
            shim_cursor_row = in->h.dh;
            shim_cursor_col = in->h.dl;
        }
    }
    *out = *in;
    return 0;
}

void outp(unsigned port, int value)
{
    if (port == 0x3C8) { dac_index = value & 255; dac_chan = 0; }
    else if (port == 0x3C9) {
        shim_dac[dac_index][dac_chan] = (unsigned char)((value & 63) << 2);
        if (++dac_chan == 3) { dac_chan = 0; dac_index = (dac_index + 1) & 255; }
    }
}

static char keys[4096];
static int khead, ktail;
static int pushed = -1;

void shim_type(const char *s)
{
    while (*s && ktail < (int)sizeof(keys))
        keys[ktail++] = *s++;
}
static void (*on_empty)(void);
void shim_on_empty(void (*fn)(void)) { on_empty = fn; }
static int keys_left(void) { return (pushed >= 0) + (ktail - khead); }
int shim_keys_left(void) { return keys_left(); }
int kbhit(void) { return keys_left() > 0; }
int getch(void)
{
    /* Only the blocking read fires the hook: the interpreter polls kbhit
     * between statements, and that must not count as waiting. */
    if (keys_left() == 0 && on_empty) { void (*fn)(void) = on_empty; on_empty = 0; fn(); }
    if (pushed >= 0) { int c = pushed; pushed = -1; return c; }
    if (khead < ktail) return (unsigned char)keys[khead++];
    /* Nothing scripted: the harness never asks for more than it typed. */
    fprintf(stderr, "dossim: getch with no keys scripted\n");
    return 3;
}
int ungetch(int c) { pushed = c; return c; }
