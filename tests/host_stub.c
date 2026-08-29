/* Unit tests link the interpreter without a front end; these stand in for
 * the input side, which none of them exercise. */
#include "../src/host.h"

int host_getline(char *buf, int max) { (void)buf; (void)max; return 0; }
int host_getkey(void) { return 0; }
int host_echoes(void) { return 0; }
int host_pollkey(void) { return 0; }
int host_break(void) { return 0; }
