/* The PC speaker, driven the way the Apple's was.
 *
 * A click is one flip of the speaker data bit on port 61h, which is the
 * same thing $C030 did. The bell is the ROM's: a kilohertz for a tenth of
 * a second, made here by the timer's second channel, which the PC's own
 * BIOS beep uses.
 */
#include "sound.h"

#include <conio.h>
#include <time.h>

void snd_click(void)
{
    outp(0x61, (inp(0x61) & ~1) ^ 2);
}

void snd_bell(void)
{
    clock_t until;

    /* Timer channel 2, square wave, 1193182 / 1000 for a kilohertz. */
    outp(0x43, 0xB6);
    outp(0x42, 0xA9);
    outp(0x42, 0x04);
    outp(0x61, inp(0x61) | 3);           /* gate the timer to the speaker */
    /* Two ticks of the 18.2 Hz clock is about a tenth of a second. */
    until = clock() + CLOCKS_PER_SEC / 9;
    while (clock() < until)
        ;
    outp(0x61, inp(0x61) & ~3);
}
