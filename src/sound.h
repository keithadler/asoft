/* sound.h - the speaker.
 *
 * The Apple had one bit of sound: any access to $C030 flipped the cone,
 * and every tone, click and beep the machine ever made was a program
 * flipping it at the right rate. The PC has the same thing behind port
 * 61h, so on DOS this is the real speaker; the host build has nothing to
 * flip and says nothing.
 */
#ifndef ASOFT_SOUND_H
#define ASOFT_SOUND_H

void snd_click(void);            /* PEEK(-16336): one flip of the cone */
void snd_bell(void);             /* CALL -198, CALL -1052: the ROM's beep */

#endif
