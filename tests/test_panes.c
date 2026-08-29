/* The Machine pane is the reason the memory image exists, so check it shows
 * real values rather than plausible ones. The expectations come from the
 * mockup's own arithmetic: its "Control stack 52/240" is one FOR frame (18),
 * one GOSUB (6) and seven leaked ONERR frames (4 each). */
#include "../src/a2mem.h"
#include "../src/bugs.h"
#include "../src/interp.h"
#include "../src/panes.h"
#include "../src/screen.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;
static void sink(char ch) { (void)ch; }

static const char *find(pane_line *pl, int n, const char *prefix)
{
    int i;
    for (i = 0; i < n; i++)
        if (strncmp(pl[i].text, prefix, strlen(prefix)) == 0)
            return pl[i].text;
    return 0;
}

static void chk(pane_line *pl, int n, const char *prefix, const char *want)
{
    const char *got = find(pl, n, prefix);
    if (!got) {
        printf("  FAIL  no line starting [%s]\n", prefix);
        failures++;
    } else if (strcmp(got, want) != 0) {
        printf("  FAIL  want [%s]\n        got  [%s]\n", want, got);
        failures++;
    }
}

int main(void)
{
    pane_line pl[PANE_MAXLINES];
    int n;

    scr_init(sink);
    it_init();

    n = pane_machine(pl, PANE_MAXLINES);
    chk(pl, n, "TXTTAB", "TXTTAB   $0801");
    chk(pl, n, "HIMEM",  "HIMEM    $9600");
    chk(pl, n, "FRE(0)", "FRE(0)   36349");   /* empty program, as measured */
    chk(pl, n, "Control stack", "Control stack      0/240");
    chk(pl, n, "Last error", "Last error   none");
    chk(pl, n, "  ONERR leak", "  ONERR leak         ON");

    /* Run something that leaks, and check the pane reports the leak and the
     * error the way the mockup does. */
    it_line("10 ONERR GOTO 100");
    it_line("20 X = 1/0");
    it_line("100 GOTO 20");
    it_line("RUN");

    n = pane_machine(pl, PANE_MAXLINES);
    chk(pl, n, "Control stack", "Control stack      240/240");
    chk(pl, n, "  ONERR  leaked", "  ONERR  leaked   60 x 4");
    chk(pl, n, "Last error", "Last error   133");
    chk(pl, n, "  DIVISION", "  DIVISION BY ZERO");

    /* Switching a bug off has to show. */
    bug_enabled[BUG_ONERR_LEAK] = 0;
    n = pane_machine(pl, PANE_MAXLINES);
    chk(pl, n, "  ONERR leak", "  ONERR leak         off");
    bug_enabled[BUG_ONERR_LEAK] = 1;

    printf("test_panes: %s\n", failures ? "FAILED" : "ok");
    return failures ? 1 : 0;
}
