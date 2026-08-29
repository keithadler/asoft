/* DOS backend: the cell buffer straight into text video memory.
 *
 * 80x25 cannot hold a 40x24 Apple screen plus a frame, a menu bar and a
 * status line, so this loads the 8x8 font at startup. That gives 43 rows on
 * EGA and 50 on VGA; either way the layout uses the top 43 and the rest is
 * left blank, which avoids caring which one is underneath.
 *
 * Attributes are already CGA bytes, so a cell is a character and its
 * attribute -- exactly the pair the hardware stores -- and the flush is a
 * copy. Every background used is below 8, so nothing lands in the blink bit.
 */
#include "tui.h"

#include <dos.h>
#include <conio.h>
#include <string.h>

#define VID_COLS 80

static unsigned char far *vid = (unsigned char far *)0xB8000000L;
static unsigned char ch_buf[TUI_H][TUI_W];
static unsigned char at_buf[TUI_H][TUI_W];
static int cur_x = -1, cur_y = -1;
static int saved_mode = 3;

/* CP437 has the box drawing built in, which is what these glyphs were drawn
 * from in the first place. */
static unsigned char glyph(int ch)
{
    switch (ch) {
    case G_TL:    return 0xC9;
    case G_TR:    return 0xBB;
    case G_BL:    return 0xC8;
    case G_BR:    return 0xBC;
    case G_HORZ:  return 0xCD;
    case G_VERT:  return 0xBA;
    case G_SHADE: return 0xB0;
    case G_RULE:  return 0xC4;
    default:      return (unsigned char)((ch < 32) ? ' ' : ch);
    }
}

static void set_cursor_shape(int visible)
{
    union REGS r;
    r.h.ah = 1;
    if (visible) { r.h.ch = 6; r.h.cl = 7; }
    else         { r.h.ch = 0x20; r.h.cl = 0; }   /* bit 5 hides it */
    int86(0x10, &r, &r);
}

void tui_init(void)
{
    union REGS r;

    r.h.ah = 0x0F;                     /* remember what we found */
    int86(0x10, &r, &r);
    saved_mode = r.h.al;

    r.x.ax = 0x0003;                   /* 80x25 colour text */
    int86(0x10, &r, &r);

    r.x.ax = 0x1112;                   /* load the 8x8 font: 43 or 50 rows */
    r.h.bl = 0;
    int86(0x10, &r, &r);

    set_cursor_shape(0);
    memset(ch_buf, ' ', sizeof(ch_buf));
    memset(at_buf, 7, sizeof(at_buf));
}

void tui_shutdown(void)
{
    union REGS r;
    r.x.ax = (unsigned short)saved_mode;
    int86(0x10, &r, &r);
    set_cursor_shape(1);
}

void tui_clear(unsigned char attr)
{
    memset(ch_buf, ' ', sizeof(ch_buf));
    memset(at_buf, attr, sizeof(at_buf));
}

void tui_put(int x, int y, int ch, unsigned char attr)
{
    if (x < 0 || x >= TUI_W || y < 0 || y >= TUI_H)
        return;
    ch_buf[y][x] = (unsigned char)ch;
    at_buf[y][x] = attr;
}

void tui_puts(int x, int y, const char *s, unsigned char attr)
{
    while (*s && x < TUI_W)
        tui_put(x++, y, (unsigned char)*s++, attr);
}

void tui_fill(int x, int y, int w, int h, int ch, unsigned char attr)
{
    int i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            tui_put(x + i, y + j, ch, attr);
}

void tui_box(int x, int y, int w, int h, const char *title, unsigned char attr)
{
    int i, len, tx;

    tui_put(x, y, G_TL, attr);
    tui_put(x + w - 1, y, G_TR, attr);
    tui_put(x, y + h - 1, G_BL, attr);
    tui_put(x + w - 1, y + h - 1, G_BR, attr);
    for (i = 1; i < w - 1; i++) {
        tui_put(x + i, y, G_HORZ, attr);
        tui_put(x + i, y + h - 1, G_HORZ, attr);
    }
    for (i = 1; i < h - 1; i++) {
        tui_put(x, y + i, G_VERT, attr);
        tui_put(x + w - 1, y + i, G_VERT, attr);
    }
    if (title && *title) {
        len = (int)strlen(title);
        tx = x + (w - len - 2) / 2;
        tui_put(tx, y, ' ', attr);
        tui_puts(tx + 1, y, title, attr);
        tui_put(tx + len + 1, y, ' ', attr);
    }
}

int tui_cell(int x, int y)
{
    if (x < 0 || x >= TUI_W || y < 0 || y >= TUI_H)
        return ' ';
    return ch_buf[y][x];
}

void tui_cursor(int x, int y) { cur_x = x; cur_y = y; }

void tui_flush(void)
{
    union REGS r;
    int y, x;

    for (y = 0; y < TUI_H; y++) {
        unsigned char far *p = vid + (unsigned)y * VID_COLS * 2;
        for (x = 0; x < TUI_W; x++) {
            *p++ = glyph(ch_buf[y][x]);
            *p++ = at_buf[y][x];
        }
    }
    if (cur_x >= 0 && cur_y >= 0) {
        set_cursor_shape(1);
        r.h.ah = 2;
        r.h.bh = 0;
        r.h.dh = (unsigned char)cur_y;
        r.h.dl = (unsigned char)cur_x;
        int86(0x10, &r, &r);
    } else {
        set_cursor_shape(0);
    }
}

/* --- keys --------------------------------------------------------------- */

/* Extended keys arrive with a zero ASCII byte and the scan code beside it. */
static int from_scan(int scan)
{
    switch (scan) {
    case 0x48: return K_UP;
    case 0x50: return K_DOWN;
    case 0x4B: return K_LEFT;
    case 0x4D: return K_RIGHT;
    case 0x47: return K_HOME;
    case 0x4F: return K_END;
    case 0x49: return K_PGUP;
    case 0x51: return K_PGDN;
    case 0x53: return K_DEL;
    case 0x3B: return K_F1;
    case 0x3C: return K_F2;
    case 0x3D: return K_F3;
    case 0x3E: return K_F4;
    case 0x3F: return K_F5;
    case 0x40: return K_F6;
    case 0x41: return K_F7;
    case 0x42: return K_F8;
    case 0x43: return K_F9;
    case 0x44: return K_F10;
    default:   return 0;
    }
}

/* int 16h reports "nothing waiting" in the zero flag, which int86 does not
 * hand back, so use the runtime's own console calls instead. getch returns a
 * zero byte before an extended key's scan code. */
int tui_haskey(void)
{
    return kbhit() ? 1 : 0;
}

int tui_getkey(void)
{
    int c, k;

    for (;;) {
        c = getch();
        if (c != 0 && c != 0xE0)
            return c;
        k = from_scan(getch());
        if (k)
            return k;
        /* an extended key nothing is bound to; wait for the next one */
    }
}
