/* The console's pieces that are portable: the text page at $400, the video
 * soft switches, the screen model's cursor and window, and the rendering of
 * a page row through the character generator. Each check is decided by the
 * thing under test -- a byte read back from the image, a switch's effect on
 * what the display would paint -- not by something beside it. */
#include "../src/a2mem.h"
#include "../src/appletext.h"
#include "../src/gfx.h"
#include "../src/hires.h"
#include "../src/screen.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void chk(const char *what, long got, long want)
{
    if (got != want) {
        printf("  FAIL  %-36s want %ld got %ld\n", what, want, got);
        failures++;
    }
}

/* --- a change hook that counts full repaints and remembers bytes ------- */
static int full_repaints;
static a2addr last_byte;
static void hook(a2addr a)
{
    if (a == 0) full_repaints++;
    else last_byte = a;
}

/* --- a sink that records what the model emitted ------------------------ */
static char emitted[512];
static int nemitted;
static void sink(char ch) { if (nemitted < 511) emitted[nemitted++] = ch; }
static int count(char ch)
{
    int i, n = 0;
    for (i = 0; i < nemitted; i++) if (emitted[i] == ch) n++;
    return n;
}
static int cursor_moves;
static void moved(void) { cursor_moves++; }

static void test_text_page(void)
{
    a2_init();
    gfx_reset();
    gfx_on_change(hook);

    gfx_text_put(0, 0, (unsigned char)('A' | 0x80));
    chk("put row 0 col 0 -> $400", a2_peek(0x0400), 0xC1);
    chk("put reported to the display", last_byte, 0x0400);
    gfx_text_put(1, 39, (unsigned char)('Z' | 0x80));
    chk("put row 1 col 39 -> $4A7", a2_peek(0x04A7), 0xDA);
    gfx_text_put(23, 0, (unsigned char)('B' | 0x80));
    chk("put row 23 -> $7D0", a2_peek(0x07D0), 0xC2);
    chk("get reads it back", gfx_text_get(23, 0), 0xC2);

    /* A clear fills the rows and leaves the holes between them alone. */
    a2_poke(0x0478, 0x55);
    full_repaints = 0;
    gfx_text_clear(0, 24, 0, 40);
    chk("clear: row 0 blank", a2_peek(0x0400), TEXT_BLANK);
    chk("clear: row 23 blank", a2_peek(0x07D0), TEXT_BLANK);
    chk("clear: screen hole untouched", a2_peek(0x0478), 0x55);
    chk("clear asks for a repaint", full_repaints, 1);

    /* Scrolling a four-line window moves those rows and nothing above. */
    gfx_text_put(19, 0, 0xD9);        /* Y */
    gfx_text_put(21, 5, 0xD8);        /* X */
    gfx_text_put(23, 7, 0xD7);        /* W */
    gfx_text_scroll(20, 24, 0, 40);
    chk("scroll(20): row 21 moved to 20", gfx_text_get(20, 5), 0xD8);
    chk("scroll(20): row 23 moved to 22", gfx_text_get(22, 7), 0xD7);
    chk("scroll(20): row 23 now blank", gfx_text_get(23, 7), TEXT_BLANK);
    chk("scroll(20): row 19 untouched", gfx_text_get(19, 0), 0xD9);
    chk("scroll(20): row 21 col 5 blank", gfx_text_get(21, 5), TEXT_BLANK);

    /* A whole-screen scroll takes row 0 with it. */
    gfx_text_scroll(0, 24, 0, 40);
    chk("scroll(0): row 19 moved to 18", gfx_text_get(18, 0), 0xD9);
    chk("scroll(0): row 19 now blank", gfx_text_get(19, 0), TEXT_BLANK);
}

static void test_switches(void)
{
    a2_init();
    gfx_reset();
    gfx_on_change(hook);
    chk("reset: text", gfx_mode(), GFX_TEXT);
    chk("reset: not mixed", gfx_mixed(), 0);

    gfx_hgr();
    chk("HGR: hi-res", gfx_mode(), GFX_HIRES);
    chk("HGR: mixed", gfx_mixed(), 1);
    chk("HGR: shows page 1", gfx_disp_hpage(), HIRES_PAGE1);
    chk("HGR: plots page 1", gfx_hpage(), HIRES_PAGE1);

    /* Full screen by hand, then the page flip that double-buffering used:
     * draw on 1, show 2. */
    full_repaints = 0;
    gfx_softswitch(SW_MIXOFF);
    chk("POKE -16302: not mixed", gfx_mixed(), 0);
    chk("POKE -16302: repaint", full_repaints, 1);
    gfx_softswitch(SW_MIXOFF);
    chk("same switch again: no repaint", full_repaints, 1);
    gfx_softswitch(SW_PAGE2);
    chk("POKE -16299: shows page 2", gfx_disp_hpage(), HIRES_PAGE2);
    chk("POKE -16299: still plots page 1", gfx_hpage(), HIRES_PAGE1);
    chk("page flip repaints", full_repaints, 2);

    /* What the display would paint follows the shown page, not the drawn
     * one: a dot on page 1 is invisible while page 2 is up. */
    gfx_hcolor(3);
    gfx_hplot(10, 10);
    chk("dot landed on page 1", gfx_hscrn(10, 10), 1);
    chk("but page 2 is showing: black", hires_pixel(10, 10), PAL_HIRES + 0);
    gfx_softswitch(SW_PAGE1);
    /* A lone dot on an even column is violet: the colour is the position. */
    chk("back on page 1: violet", hires_pixel(10, 10), PAL_HIRES + 2);
    {
        int y, col;
        chk("locate on the shown page", gfx_hires_locate(0x2000, &y, &col), 1);
        gfx_softswitch(SW_PAGE2);
        chk("locate: page 1 byte not shown", gfx_hires_locate(0x2000, &y, &col), 0);
        chk("locate: page 2 byte shown", gfx_hires_locate(0x4000, &y, &col), 1);
    }

    gfx_hgr2();
    chk("HGR2: hi-res", gfx_mode(), GFX_HIRES);
    chk("HGR2: full screen", gfx_mixed(), 0);
    chk("HGR2: shows page 2", gfx_disp_hpage(), HIRES_PAGE2);
    chk("HGR2: plots page 2", gfx_hpage(), HIRES_PAGE2);
    gfx_softswitch(SW_MIXON);
    chk("POKE -16301 after HGR2: mixed", gfx_mixed(), 1);

    /* TEXT flips one switch and leaves the rest, as SETTXT did. */
    gfx_text();
    chk("TEXT: text", gfx_mode(), GFX_TEXT);
    chk("TEXT: mixed bit still set", gfx_mixed(), 1);
    gfx_softswitch(SW_TEXTOFF);
    chk("POKE -16304: graphics again, hi-res", gfx_mode(), GFX_HIRES);
    gfx_softswitch(SW_LORES);
    chk("POKE -16298: lo-res", gfx_mode(), GFX_LORES);
    gfx_softswitch(SW_TEXTON);

    gfx_gr();
    chk("GR: lo-res", gfx_mode(), GFX_LORES);
    chk("GR: mixed", gfx_mixed(), 1);
    chk("GR: text page rows 0-19 cleared", a2_peek(gfx_text_row(19)), 0);

    chk("is_softswitch $C050", gfx_is_softswitch(0xC050), 1);
    chk("is_softswitch $C057", gfx_is_softswitch(0xC057), 1);
    chk("is_softswitch $C058", gfx_is_softswitch(0xC058), 0);
    chk("is_softswitch $C010 (keyboard)", gfx_is_softswitch(0xC010), 0);
}

static void test_screen_model(void)
{
    /* Without a cursor hook the model is a stream: VTAB can only print
     * its way down, and the bottom-line move is nothing at all. */
    nemitted = 0;
    scr_init(sink);
    scr_vtab(4);
    chk("stream VTAB 4 prints 3 newlines", count('\n'), 3);
    chk("stream VTAB 4: row 3", scr_row(), 3);
    nemitted = 0;
    scr_vtab(2);
    chk("stream VTAB up prints nothing", nemitted, 0);
    scr_cursor_bottom();
    chk("stream cursor_bottom: no move", scr_row(), 3);
    chk("stream cursor_bottom: no output", nemitted, 0);

    /* With one, the cursor is real. */
    nemitted = 0;
    cursor_moves = 0;
    scr_init(sink);
    scr_on_cursor(moved);
    scr_vtab(4);
    chk("VTAB 4: row 3", scr_row(), 3);
    chk("VTAB 4: prints nothing", nemitted, 0);
    chk("VTAB 4: hook fired", cursor_moves, 1);
    scr_vtab(2);
    chk("VTAB 2: up to row 1", scr_row(), 1);
    scr_vtab(99);
    chk("VTAB 99: clamps to 23", scr_row(), 23);
    scr_puts("HELLO");
    scr_htab(10);
    chk("HTAB 10: col 9", scr_col(), 9);
    chk("HTAB 10: prints nothing", nemitted, 5);
    scr_cursor_bottom();
    chk("cursor_bottom: row 23", scr_row(), 23);
    chk("cursor_bottom: col kept", scr_col(), 9);

    /* HOME clears to the window top, not the screen top. */
    scr_window(20);
    chk("window top 20", scr_window_top(), 20);
    nemitted = 0;
    scr_home();
    chk("HOME in a window: row 20", scr_row(), 20);
    chk("HOME in a window: col 0", scr_col(), 0);
    chk("HOME emits one clear", count('\f'), 1);
    scr_window(0);
    scr_home();
    chk("HOME full screen: row 0", scr_row(), 0);

    /* PR#3 resets the window and homes; PR#0 the same. */
    scr_window(20);
    scr_vtab(22);
    nemitted = 0;
    scr_set_cols(80);
    chk("PR#3: 80 columns", scr_cols(), 80);
    chk("PR#3: window reset", scr_window_top(), 0);
    chk("PR#3: row 0", scr_row(), 0);
    chk("PR#3: one clear", count('\f'), 1);
    nemitted = 0;
    scr_set_cols(80);
    chk("PR#3 twice: nothing", nemitted, 0);
    scr_set_cols(40);
    chk("PR#0: 40 columns", scr_cols(), 40);

    scr_text_mode(SCR_INVERSE);
    chk("INVERSE remembered", scr_get_text_mode(), SCR_INVERSE);
    scr_text_mode(SCR_NORMAL);
    chk("NORMAL remembered", scr_get_text_mode(), SCR_NORMAL);
}

static int lit_pixels(const unsigned char *out, int x0, int x1)
{
    int x, n = 0;
    for (x = x0; x <= x1; x++)
        if (out[x] != PAL_HIRES + 0) n++;
    return n;
}

static void test_page_render(void)
{
    unsigned char out[ATEXT_W];
    int line, glyph_dots = 0, inverse_dots = 0;

    a2_init();
    gfx_reset();
    gfx_on_change(0);
    gfx_text_clear(0, 24, 0, 40);

    /* A normal blank is dark; an inverse blank is a solid white cell, seven
     * pixels wide, on every one of its eight lines -- gap columns too. */
    gfx_text_put(20, 0, TEXT_BLANK);
    gfx_text_put(20, 1, 0x20);
    for (line = 0; line < 8; line++) {
        appletext_page_row(20 * 8 + line, out);
        chk("normal blank cell: dark", lit_pixels(out, 0, 6), 0);
        chk("inverse blank cell: all lit", lit_pixels(out, 7, 13), 7);
        chk("inverse blank cell: white", out[10], PAL_HIRES + 3);
    }

    /* An A has dots; its inverse has the complementary ones, so between
     * them a cell's seven columns are all accounted for on each line. */
    gfx_text_put(21, 0, 0xC1);
    gfx_text_put(21, 1, 0x01);
    for (line = 0; line < 8; line++) {
        appletext_page_row(21 * 8 + line, out);
        glyph_dots += lit_pixels(out, 0, 6);
        inverse_dots += lit_pixels(out, 7, 13);
    }
    chk("A has dots", glyph_dots > 0, 1);
    chk("A has gaps", glyph_dots < 56, 1);
    /* Fringing widens a lone dot to two pixels, so the two counts need
     * not sum to exactly 56; but the inverse must be the busier cell. */
    chk("inverse A lights more than A", inverse_dots > glyph_dots, 1);

    /* The sixty-four glyphs: $41, $01 and $C1 are the same letter. */
    {
        unsigned char a[ATEXT_W], b[ATEXT_W];
        gfx_text_put(22, 0, 0xC1);
        gfx_text_put(22, 1, 0x41);            /* flashing A: drawn inverse */
        appletext_page_row(22 * 8 + 2, a);
        appletext_page_row(21 * 8 + 2, b);
        chk("flash A drawn as inverse A", memcmp(a + 7, b + 7, 7), 0);
    }
}

int main(void)
{
    test_text_page();
    test_switches();
    test_screen_model();
    test_page_render();
    if (failures) {
        printf("test_console: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_console: ok\n");
    return 0;
}
