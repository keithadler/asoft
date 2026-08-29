/* The text screen is rendered as dots and run through the artifact rules, so
 * a character's colour depends on where it sits, exactly as hi-res does. The
 * letter H is a good witness: its uprights are single columns, which come out
 * coloured, and its crossbar is a run, which comes out white. */
#include "../src/appletext.h"
#include "../src/gfx.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;
static char cells[ATEXT_ROWS][ATEXT_COLS];
static unsigned char row[ATEXT_W];

static const char *name(int idx)
{
    switch (idx - PAL_HIRES) {
    case 0: return "black";
    case 1: return "green";
    case 2: return "violet";
    case 3: return "white";
    case 4: return "orange";
    case 5: return "blue";
    default: return "?";
    }
}

static void px(const char *what, int x, int want)
{
    if (row[x] != want) {
        printf("  FAIL  %s: x=%d is %s, want %s\n", what, x,
               name(row[x]), name(want));
        failures++;
    }
}

int main(void)
{
    int r, x, lit;

    for (r = 0; r < ATEXT_ROWS; r++)
        memset(cells[r], ' ', ATEXT_COLS);

    /* A blank screen has to be entirely black, or the fringing would show up
     * on nothing at all. */
    appletext_row(&cells[0][0], 0, row);
    for (x = 0; x < ATEXT_W; x++)
        if (row[x] != PAL_HIRES + 0) {
            printf("  FAIL  blank screen has a lit pixel at x=%d\n", x);
            failures++;
            break;
        }

    cells[0][0] = 'H';

    /* Top scan line of H is two uprights: dots at x=0 and x=4, each alone. A
     * lone dot takes its column's colour and fills the whole colour cycle, so
     * each becomes two violet pixels rather than one white one. */
    appletext_row(&cells[0][0], 0, row);
    px("H upright", 0, PAL_HIRES + 2);
    px("H upright widened", 1, PAL_HIRES + 2);
    px("gap between uprights", 2, PAL_HIRES + 0);
    px("gap between uprights", 3, PAL_HIRES + 0);
    px("H upright", 4, PAL_HIRES + 2);
    px("H upright widened", 5, PAL_HIRES + 2);
    px("cell is seven wide", 6, PAL_HIRES + 0);

    /* The crossbar is five dots in a row: neighbours on both sides, so white. */
    appletext_row(&cells[0][0], 3, row);
    for (x = 0; x < 5; x++)
        px("H crossbar", x, PAL_HIRES + 3);
    px("crossbar ends", 5, PAL_HIRES + 0);

    /* The eighth scan line is the gap between rows, and must be blank. */
    appletext_row(&cells[0][0], 7, row);
    for (x = 0, lit = 0; x < ATEXT_W; x++)
        if (row[x] != PAL_HIRES + 0) lit++;
    if (lit) {
        printf("  FAIL  the row gap has %d lit pixels\n", lit);
        failures++;
    }

    /* The same letter one column right comes out green rather than violet: a
     * cell is seven pixels and seven is odd, so a glyph's dots land on the
     * other parity in every other column. That alternation is why a screen of
     * Apple text shimmered green and violet rather than being one colour.
     *
     * And the widening runs leftwards here, because the colour cycles pair
     * (0,1), (2,3) and so on -- so x=7 fills x=6, which belongs to the
     * character before it. Colour bleeding across a character boundary is not
     * an error in the model; it is the thing being modelled. */
    cells[0][0] = ' ';
    cells[0][1] = 'H';
    appletext_row(&cells[0][0], 0, row);
    px("H in column 1", 7, PAL_HIRES + 1);
    px("bleeds into the cell before it", 6, PAL_HIRES + 1);

    printf("test_appletext: %s\n", failures ? "FAILED" : "ok");
    return failures ? 1 : 0;
}
