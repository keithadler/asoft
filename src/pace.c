#include "pace.h"

#ifdef __DOS__
#include <time.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

static long rate = PACE_APPLE_RATE;
static long counted;
static long start_us;

/* Wall clock in microseconds. DOS has no multitasking, so its clock() is wall
 * time and the 55ms tick is fine for pacing; elsewhere clock() would measure
 * processor time, which stops advancing the moment we sleep. */
static long now_us(void)
{
#ifdef __DOS__
    return (long)((double)clock() * 1000000.0 / (double)CLOCKS_PER_SEC);
#else
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (long)tv.tv_sec * 1000000L + (long)tv.tv_usec;
#endif
}

static void sleep_us(long us)
{
    if (us <= 0)
        return;
#ifdef __DOS__
    {
        /* Nothing else wants the processor, and DOS has no sleep worth the
         * name, so wait for the clock to catch up. */
        long until = now_us() + us;
        while (now_us() < until)
            ;
    }
#else
    usleep((unsigned)us);
#endif
}

void pace_set_rate(long statements_per_second)
{
    rate = statements_per_second < 0 ? 0 : statements_per_second;
    pace_reset();
}

long pace_rate(void) { return rate; }

void pace_reset(void)
{
    counted = 0;
    start_us = now_us();
}

/* Sleep off whatever this statement was ahead by. Checking every statement
 * would cost more than it saves, so the debt is settled in small batches --
 * often enough that a program's timing loop is smooth, rarely enough that the
 * check is not the expensive part. */
void pace_statement(void)
{
    long behind;

    if (rate <= 0)
        return;
    if (++counted & 31)
        return;

    behind = (long)((double)counted * 1000000.0 / (double)rate)
           - (now_us() - start_us);
    if (behind > 0)
        sleep_us(behind);
}
