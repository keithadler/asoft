/* console_dos.h - the DOS console front end's few entry points. */
#ifndef ASOFT_CONSOLE_DOS_H
#define ASOFT_CONSOLE_DOS_H

/* Decide whether there is a screen to be a console on, and register the
 * screen model's sink accordingly. Before it_init, like any scr_init. */
void con_init(void);

/* Switch the forty-column text mode on, cleared. After it_init, which
 * wipes the memory image the text page lives in, and after disp_init and
 * gfx_on_change, because the clear paints through them. */
void con_start(void);

/* Non-zero when stdin and stdout are the real console. Redirected -- the
 * capture rig's ASOFT.EXE < SCRIPT.TXT > OUT.TXT -- the build behaves as
 * the stream it always was, so transcripts still come out. */
int  con_interactive(void);

#endif
