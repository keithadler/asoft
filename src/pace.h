/* pace.h - run at the speed the machine ran at.
 *
 * Applesoft has no clock. A program that wants to wait counts to a number in
 * a FOR loop, and how long that takes is a property of the hardware -- about
 * a thousand statements a second on a 1 MHz 6502. On anything modern the same
 * loop is instantaneous, so a game written for the Apple crosses the screen
 * before you can press a key, and the delay constant that made it playable
 * becomes meaningless: right for one machine, wrong for every other.
 *
 * So the interpreter paces itself instead. The number below is the only place
 * the speed lives, and a program's own timing loops then mean the same thing
 * on a laptop, under DOSBox, and on a real DOS machine.
 *
 * It is a limit, not a guarantee: a slow enough host simply runs slower, and
 * anything compute-bound -- a Mandelbrot -- is better off unthrottled.
 */
#ifndef ASOFT_PACE_H
#define ASOFT_PACE_H

/* Statements per second. Measured from the usual benchmark: FOR I=1 TO 1000:
 * NEXT takes something like a second and a half on an Apple II, which is a
 * bit under a thousand times round the loop. Round numbers are honest enough
 * here -- real Applesoft varied by more than this depending on the statement. */
#define PACE_APPLE_RATE 1000L

/* 0 runs flat out. */
void pace_set_rate(long statements_per_second);
long pace_rate(void);

/* Whether the pace waits to be asked for. A program that never looks at the
 * keyboard while it runs -- a Mandelbrot, a dragon curve -- gains nothing
 * from the machine's speed except minutes of blank screen, so by default
 * the pace is held back until the program reads the keyboard strobe, which
 * is what a game does to keep moving between keys. From then on its delay
 * loops mean what they meant. pace_set_always makes it apply from the first
 * statement regardless. */
void pace_set_always(int always);
void pace_engage(void);

/* Called when a program starts, and once per statement while it runs. */
void pace_reset(void);
void pace_statement(void);

/* Every statement ever executed, paced or not: what -b divides by time. */
long pace_total(void);

/* The wall clock, in microseconds, portably: gettimeofday where there is
 * one, the 55ms DOS tick where there is not. Here because pace.c already
 * had to solve this and a display wanting to rate-limit itself is the same
 * problem. */
long pace_now_us(void);

#endif
