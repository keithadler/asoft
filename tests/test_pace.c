/* The pace waits to be asked for: flat out until a program reads the
 * keyboard, the machine's speed from then on, and the time already run
 * flat out is not slept off when it engages. */
#include "../src/pace.h"
#include <stdio.h>

static int failures;
static void chk(const char *what, int ok)
{
    if (!ok) { printf("  FAIL  %s\n", what); failures++; }
}

/* Microseconds to run n statements through the pacer. */
static long run(int n)
{
    long t0 = pace_now_us();
    int i;
    for (i = 0; i < n; i++)
        pace_statement();
    return pace_now_us() - t0;
}

int main(void)
{
    long t;

    pace_set_rate(1000);
    pace_reset();
    t = run(640);                 /* 640 ms at the machine's pace */
    chk("not engaged: flat out (under 50ms for 640 statements)", t < 50000L);

    pace_engage();
    t = run(96);                  /* 96 ms at the machine's pace */
    chk("engaged: paced (at least 60ms for 96 statements)", t >= 60000L);
    chk("engaged: no catch-up debt from the flat-out stretch", t < 400000L);

    pace_reset();                 /* RUN again */
    t = run(640);
    chk("RUN resets to flat out", t < 50000L);

    pace_set_always(1);
    pace_reset();
    t = run(96);
    chk("-p: paced from the first statement", t >= 60000L);

    pace_set_always(0);
    pace_set_rate(0);
    pace_engage();
    t = run(640);
    chk("-f: flat out even when engaged", t < 50000L);

    if (failures) { printf("test_pace: %d failure(s)\n", failures); return 1; }
    printf("test_pace: ok\n");
    return 0;
}
