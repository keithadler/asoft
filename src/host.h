/* host.h - the few things the interpreter needs from whatever is driving it.
 * The stdio build reads the real stdin; the Turbo Vision build pumps its
 * event loop and returns what the user typed into the Apple ][ window. */
#ifndef ASOFT_HOST_H
#define ASOFT_HOST_H

/* Read a line for INPUT. Returns 0 on end of input. */
int host_getline(char *buf, int max);

/* Read a single key for GET. Returns 0 on end of input. */
int host_getkey(void);

/* Non-zero when the user has pressed Ctrl-C since the last call. */
int host_break(void);

#endif
