/* errs.h - Applesoft error codes and their messages.
 *
 * The numbers are the ROM's, which matters because ONERR programs read them
 * back with PEEK(222): division by zero really is 133, and the mockup's
 * Machine pane shows exactly that. The order below is the order the messages
 * appear in the ROM's table. */
#ifndef ASOFT_ERRS_H
#define ASOFT_ERRS_H

#define ERR_NONE          -1
#define ERR_NEXTWITHOUTFOR  0
#define ERR_SYNTAX         16
#define ERR_RETWITHOUTGOSUB 22
#define ERR_OUTOFDATA      42
#define ERR_ILLEGALQTY     53
#define ERR_OVERFLOW       69
#define ERR_OUTOFMEM       77
#define ERR_UNDEFSTMT      90
#define ERR_BADSUBSCRIPT  107
#define ERR_REDIMD        120
#define ERR_DIVBYZERO     133
#define ERR_TYPEMISMATCH  163
#define ERR_STRINGTOOLONG 176
#define ERR_FORMULATOOCOMPLEX 191
#define ERR_UNDEFFUNC     224
#define ERR_REENTER       254
#define ERR_BREAK         255

/* "DIVISION BY ZERO" for 133, and so on. Never returns NULL. */
const char *err_message(int code);

#endif
