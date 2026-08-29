/* Terminal display for the native build.
 *
 * Draws the graphics page with half-block characters: one cell carries two
 * vertical pixels, the top in the foreground colour and the bottom in the
 * background. With 24-bit colour that gives a real picture in a terminal.
 *
 * Hi-res is 280x192, which is far too big for a terminal at one cell per
 * pixel, so it is halved in both directions -- 140 columns by 48 rows. Lo-res
 * is 40x48 and needs no scaling at all.
 *
 * Unlike the DOS build this cannot take over the screen, so it repaints on
 * demand rather than following every plotted pixel: the front end asks once,
 * when a program stops and the prompt comes back.
 */
#include "display.h"
#include "gfx.h"
#include "hires.h"

#include <stdio.h>
#include <string.h>

static int dirty;
static int shown_mode = GFX_TEXT;

void disp_init(void)
{
    dirty = 0;
    shown_mode = GFX_TEXT;
}

void disp_shutdown(void) { }

void disp_touch(a2addr changed)
{
    (void)changed;
    if (gfx_mode() != shown_mode) {
        shown_mode = gfx_mode();
        dirty = 1;
        return;
    }
    if (gfx_mode() != GFX_TEXT)
        dirty = 1;
}

static void put_pair(int top, int bottom)
{
    const unsigned char *t = gfx_palette[top];
    const unsigned char *b = gfx_palette[bottom];
    printf("\033[38;2;%d;%d;%dm\033[48;2;%d;%d;%dm\xe2\x96\x80",
           t[0], t[1], t[2], b[0], b[1], b[2]);
}

static void draw_hires(void)
{
    unsigned char a[HIRES_W], b[HIRES_W];
    int y, x;

    for (y = 0; y < HIRES_H; y += 4) {
        hires_row(y, a);
        hires_row(y + 2 < HIRES_H ? y + 2 : y, b);
        for (x = 0; x < HIRES_W; x += 2)
            put_pair(a[x], b[x]);
        printf("\033[0m\n");
    }
}

static void draw_lores(void)
{
    int y, x;
    for (y = 0; y < LORES_H; y += 2) {
        for (x = 0; x < LORES_W; x++)
            put_pair(lores_pixel(x, y), lores_pixel(x, y + 1));
        printf("\033[0m\n");
    }
}

void disp_refresh(void)
{
    if (!dirty)
        return;
    dirty = 0;
    if (gfx_mode() == GFX_HIRES)
        draw_hires();
    else if (gfx_mode() == GFX_LORES)
        draw_lores();
}

int disp_graphics(void)
{
    return gfx_mode() != GFX_TEXT;
}

int disp_suppress_text(void)
{
    return 0;
}
