/* printer.h - PR#1.
 *
 * Slot 1 held the printer card. PR#1 sent everything printed to it until
 * PR#0 brought the screen back, and on DOS that is exactly what the PRN
 * device is, so the DOS build prints for real. The host build has no
 * parallel port and appends to printer.txt instead, which is also what the
 * tests read.
 */
#ifndef ASOFT_PRINTER_H
#define ASOFT_PRINTER_H

void printer_on(void);
void printer_off(void);

#endif
