/* interp.h - the interpreter proper: expressions, statements, control flow.
 *
 * The front end owns the REPL loop and the screen; this owns everything
 * between reading a line and the output appearing. Both the stdio and the
 * Turbo Vision builds link against exactly this.
 */
#ifndef ASOFT_INTERP_H
#define ASOFT_INTERP_H

#include "a2mem.h"

/* Frames on the control stack, sized in bytes so the Machine pane can show a
 * meaningful "52/240" the way the ROM's page-1 stack would. */
#define CSTACK_BYTES 240
#define FRAME_FOR    0
#define FRAME_GOSUB  1
#define FRAME_ONERR  2

void it_init(void);

/* Hand one line of user input to the interpreter. A line starting with a
 * number is stored (or deletes that line if nothing follows); anything else
 * runs immediately. */
void it_line(const char *src);

/* True once the user has asked to leave. */
int  it_quitting(void);

/* --- state, for the Machine pane and the debugger ----------------------- */
int  it_cstack_used(void);          /* bytes */
int  it_cstack_count(void);
int  it_frame_info(int i, int *kind, char *label, long *value);
int  it_leaked_frames(void);
int  it_last_error(void);           /* ERR_NONE when there has not been one */
int  it_last_error_line(void);
int  it_current_line(void);

/* --- program files ------------------------------------------------------ */
/* Both return 0 on failure. The reference console build has neither; the
 * File menu in the mockup needs both. */
int  it_load(const char *path);
int  it_save(const char *path);

#endif
