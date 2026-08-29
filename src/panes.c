#include "panes.h"
#include "a2mem.h"
#include "bugs.h"
#include "errs.h"
#include "interp.h"

#include <stdio.h>
#include <string.h>

static int add(pane_line *out, int max, int n, int style, const char *fmt,
               long a, const char *s)
{
    char buf[128];
    if (n >= max)
        return n;
    if (s)
        sprintf(buf, fmt, s);
    else if (fmt[0])
        sprintf(buf, fmt, a);
    else
        buf[0] = '\0';
    buf[PANE_WIDTH] = '\0';
    strcpy(out[n].text, buf);
    out[n].style = style;
    return n + 1;
}

int pane_machine(pane_line *out, int max)
{
    int n = 0;
    int i;

    n = add(out, max, n, PL_RULE, "", 0, 0);

    /* The zero-page pointers, in the order they carve up memory. */
    n = add(out, max, n, PL_PLAIN, "TXTTAB   $%04lX", (long)a2_word(ZP_TXTTAB), 0);
    n = add(out, max, n, PL_PLAIN, "VARTAB   $%04lX", (long)a2_word(ZP_VARTAB), 0);
    n = add(out, max, n, PL_PLAIN, "ARYTAB   $%04lX", (long)a2_word(ZP_ARYTAB), 0);
    n = add(out, max, n, PL_PLAIN, "STREND   $%04lX", (long)a2_word(ZP_STREND), 0);
    n = add(out, max, n, PL_PLAIN, "FRETOP   $%04lX", (long)a2_word(ZP_FRETOP), 0);
    n = add(out, max, n, PL_PLAIN, "HIMEM    $%04lX", (long)a2_word(ZP_MEMSIZ), 0);
    n = add(out, max, n, PL_PLAIN, "FRE(0)   %ld", a2_fre(), 0);
    n = add(out, max, n, PL_PLAIN, "", 0, 0);

    {
        char buf[64];
        sprintf(buf, "Control stack      %d/%d",
                it_cstack_used(), CSTACK_BYTES);
        n = add(out, max, n, PL_HEADING, "%s", 0, buf);
    }

    for (i = 0; i < it_cstack_count(); i++) {
        int kind;
        char label[8];
        long v;
        char buf[64];
        if (!it_frame_info(i, &kind, label, &v))
            break;
        /* Consecutive leaked ONERR frames collapse into one "7 x 4" row;
         * listing sixty of them individually would tell you nothing. */
        if (kind == FRAME_ONERR) {
            int run = 0, j;
            for (j = i; j < it_cstack_count(); j++) {
                int k2; char l2[8]; long v2;
                if (!it_frame_info(j, &k2, l2, &v2) || k2 != FRAME_ONERR)
                    break;
                run++;
            }
            sprintf(buf, "  ONERR  leaked  %3d x 4", run);
            n = add(out, max, n, PL_WARN, "%s", 0, buf);
            i = j - 1;
            continue;
        }
        if (kind == FRAME_FOR)
            sprintf(buf, "  FOR    %-6s %8ld", label, v);
        else
            sprintf(buf, "  GOSUB         %8ld", v);
        n = add(out, max, n, PL_PLAIN, "%s", 0, buf);
    }

    n = add(out, max, n, PL_PLAIN, "", 0, 0);
    n = add(out, max, n, PL_HEADING, "%s", 0, "Bugs active");
    for (i = 0; i < BUG_COUNT; i++) {
        char buf[64];
        sprintf(buf, "  %-18s %s", bug_name(i), bug_enabled[i] ? "ON" : "off");
        n = add(out, max, n, bug_enabled[i] ? PL_WARN : PL_PLAIN, "%s", 0, buf);
    }

    n = add(out, max, n, PL_PLAIN, "", 0, 0);
    if (it_last_error() == ERR_NONE) {
        n = add(out, max, n, PL_PLAIN, "%s", 0, "Last error   none");
    } else {
        char buf[64];
        sprintf(buf, "Last error   %d", it_last_error());
        n = add(out, max, n, PL_PLAIN, "%s", 0, buf);
        sprintf(buf, "  %s", err_message(it_last_error()));
        n = add(out, max, n, PL_PLAIN, "%s", 0, buf);
        if (it_last_error_line()) {
            sprintf(buf, "  in line %ld", it_last_error_line());
            n = add(out, max, n, PL_PLAIN, "%s", 0, buf);
        }
    }
    return n;
}
