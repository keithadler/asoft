/* The Apple's screen layouts are interleaved so the video hardware could be
 * built from fewer chips; consecutive rows are nowhere near each other in
 * memory. These are the canonical addresses. */
#include "../src/gfx.h"
#include "../src/a2mem.h"
#include <stdio.h>

static int failures = 0;

static void chk(const char *what, long got, long want)
{
    if (got != want) {
        printf("  FAIL  %-22s want $%04lX got $%04lX\n", what, want, got);
        failures++;
    }
}

int main(void)
{
    a2_init();
    gfx_reset();

    /* Text / lo-res: rows step by $80 within a group of eight, and each
     * group of eight starts $28 further along. */
    chk("text row 0",  gfx_text_row(0),  0x0400);
    chk("text row 1",  gfx_text_row(1),  0x0480);
    chk("text row 7",  gfx_text_row(7),  0x0780);
    chk("text row 8",  gfx_text_row(8),  0x0428);
    chk("text row 16", gfx_text_row(16), 0x0450);
    chk("text row 23", gfx_text_row(23), 0x07D0);

    /* Hi-res adds a third interleave: eight scan lines $400 apart. */
    chk("hires y 0",   gfx_hires_row(HIRES_PAGE1, 0),   0x2000);
    chk("hires y 1",   gfx_hires_row(HIRES_PAGE1, 1),   0x2400);
    chk("hires y 7",   gfx_hires_row(HIRES_PAGE1, 7),   0x3C00);
    chk("hires y 8",   gfx_hires_row(HIRES_PAGE1, 8),   0x2080);
    chk("hires y 64",  gfx_hires_row(HIRES_PAGE1, 64),  0x2028);
    chk("hires y 191", gfx_hires_row(HIRES_PAGE1, 191), 0x3FD0);
    chk("hires page2", gfx_hires_row(HIRES_PAGE2, 0),   0x4000);

    /* Two lo-res pixels share a byte: even y low, odd y high. */
    gfx_gr();
    gfx_color(5);
    gfx_plot(1, 0);
    chk("plot(1,0) byte", a2_peek(0x0401), 0x05);
    gfx_plot(1, 1);
    chk("plot(1,1) byte", a2_peek(0x0401), 0x55);
    chk("scrn(1,0)", gfx_scrn(1, 0), 5);
    chk("scrn(1,1)", gfx_scrn(1, 1), 5);

    /* Hi-res: seven pixels per byte, bit 7 picking the colour pair. */
    gfx_hgr();
    gfx_hcolor(3);
    gfx_hplot(0, 0);
    chk("hplot(0,0)", a2_peek(0x2000), 0x01);
    gfx_hplot(6, 0);
    chk("hplot(6,0)", a2_peek(0x2000), 0x41);
    gfx_hplot(7, 0);
    chk("hplot(7,0) next byte", a2_peek(0x2001), 0x01);
    gfx_hcolor(7);
    gfx_hplot(1, 0);
    chk("hcolor 7 sets bit 7", a2_peek(0x2000), 0xC3);
    gfx_hcolor(0);
    gfx_hplot(0, 0);
    chk("hcolor 0 erases", a2_peek(0x2000), 0x42);

    /* GR clears the picture but leaves the bottom four text lines. */
    gfx_hgr();
    a2_poke(gfx_text_row(22), 0xC1);
    gfx_gr();
    chk("GR keeps text rows", a2_peek(gfx_text_row(22)), 0xC1);

    printf("test_gfx: %s\n", failures ? "FAILED" : "ok");
    return failures ? 1 : 0;
}
