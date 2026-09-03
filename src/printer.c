#include "printer.h"
#include "screen.h"

#include <stdio.h>

#ifdef __DOS__
#define PRINTER_PATH "PRN"
#define PRINTER_MODE "w"
#else
#define PRINTER_PATH "printer.txt"
#define PRINTER_MODE "a"
#endif

void printer_on(void)
{
    FILE *f;
    if (scr_printer())
        return;
    f = fopen(PRINTER_PATH, PRINTER_MODE);
    /* A slot with no card in it swallowed the output; so does a printer
     * that cannot be opened. */
    if (f)
        scr_set_printer(f);
}

void printer_off(void)
{
    FILE *f = scr_printer();
    if (!f)
        return;
    scr_set_printer(0);
    fclose(f);
}
