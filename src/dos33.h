/* dos33.h - the DOS 3.3 command channel.
 *
 * Applesoft had no file statements. A program talked to DOS by printing a
 * control-D at the start of a line followed by a DOS command -- PRINT
 * CHR$(4);"OPEN SCORES" -- and DOS, which sat between BASIC and the screen,
 * took the line for itself. READ and WRITE then redirected INPUT and PRINT
 * to the file until the next command, or a bare control-D, put the screen
 * and keyboard back. Typed at the prompt, the same commands worked without
 * the control-D.
 *
 * This is that layer, over the host's files: text files as NAME.TXT,
 * programs as NAME.BAS, binary files as NAME.BIN with DOS's own four-byte
 * address and length header, in the directory the interpreter runs in. It
 * sits in the screen model's output path exactly where DOS sat, and it
 * answers with DOS's messages and ONERR codes -- "FILE NOT FOUND", code 6;
 * "END OF DATA", code 5 -- because programs checked those.
 *
 * Commands: OPEN, CLOSE, READ, WRITE, APPEND, POSITION, DELETE, RENAME,
 * CATALOG, LOAD, SAVE, RUN, BLOAD, BSAVE, PR#, IN#, and the ones that only
 * ever meant something to a floppy -- LOCK, UNLOCK, VERIFY, MON, NOMON,
 * MAXFILES, INIT, FP, INT -- which are accepted and do nothing. EXEC and
 * CHAIN are not here.
 */
#ifndef ASOFT_DOS33_H
#define ASOFT_DOS33_H

void dos_init(void);

/* The filter the screen model runs every character through. Non-zero
 * means DOS took it: as part of a command, or into the file being written. */
int  dos_filter(char ch);

/* Run one command, the text after the control-D or the line typed at the
 * prompt. Errors are raised through the hook below and do not return. */
void dos_command(const char *text);

/* Is this line a DOS command a person typed at the prompt? */
int  dos_is_command(const char *line);

/* While a READ is in effect, INPUT and GET come from the file. */
int  dos_reading(void);
int  dos_read_line(char *buf, int max);     /* 0 at end of data */
int  dos_read_char(void);                   /* -1 at end of data */

/* A DOS command reset READ and WRITE; so did an error, and so did every
 * line typed at the prompt. Files stay open until CLOSE. */
void dos_reset_modes(void);
void dos_close_all(void);

/* What the interpreter supplies: a way to raise an error with a DOS code,
 * and a way to ask for a program to be loaded or run once the statement
 * that asked has finished -- a RUN inside a PRINT cannot replace the
 * program under the PRINT's feet. */
void dos_set_error_hook(void (*raise)(int code));
void dos_set_program_hook(void (*load)(const char *path, int run));

/* Where the files are: the host path for a DOS file name, with the
 * extension DOS's file type becomes here. */
void dos_path(const char *name, const char *ext, char *out, int max);

#endif
