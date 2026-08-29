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

/* A key if one is waiting, 0 if not. Never blocks. This is what the keyboard
 * strobe at $C000 reads, and it is the difference between a program that can
 * only ask GET and wait, and a game that can keep moving while you decide. */
int host_pollkey(void);

#endif
