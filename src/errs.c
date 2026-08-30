#include "errs.h"

static const struct { int code; const char *msg; } table[] = {
    { ERR_NEXTWITHOUTFOR,   "NEXT WITHOUT FOR" },
    { ERR_SYNTAX,           "SYNTAX" },
    { ERR_RETWITHOUTGOSUB,  "RETURN WITHOUT GOSUB" },
    { ERR_OUTOFDATA,        "OUT OF DATA" },
    { ERR_ILLEGALQTY,       "ILLEGAL QUANTITY" },
    { ERR_OVERFLOW,         "OVERFLOW" },
    { ERR_OUTOFMEM,         "OUT OF MEMORY" },
    { ERR_UNDEFSTMT,        "UNDEF'D STATEMENT" },
    { ERR_BADSUBSCRIPT,     "BAD SUBSCRIPT" },
    { ERR_REDIMD,           "REDIM'D ARRAY" },
    { ERR_DIVBYZERO,        "DIVISION BY ZERO" },
    { ERR_TYPEMISMATCH,     "TYPE MISMATCH" },
    { ERR_STRINGTOOLONG,    "STRING TOO LONG" },
    { ERR_FORMULATOOCOMPLEX,"FORMULA TOO COMPLEX" },
    { ERR_UNDEFFUNC,        "UNDEF'D FUNCTION" },
    { ERR_REENTER,          "REENTER" },
    { ERR_BREAK,            "BREAK" }
};

const char *err_message(int code)
{
    int i;
    for (i = 0; i < (int)(sizeof(table) / sizeof(table[0])); i++)
        if (table[i].code == code)
            return table[i].msg;
    return "ERROR";
}
