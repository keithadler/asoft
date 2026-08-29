#include "token.h"
#include "mbf.h"

#include <string.h>
#include <ctype.h>

/* Indexed by token - TOK_FIRST. The order is the ROM's: it decides the token
 * values, and it also decides which keyword wins when two of them could match
 * at the same character. */
static const char *const keywords[] = {
    "END", "FOR", "NEXT", "DATA", "INPUT", "DEL", "DIM", "READ",
    "GR", "TEXT", "PR#", "IN#", "CALL", "PLOT", "HLIN", "VLIN",
    "HGR2", "HGR", "HCOLOR=", "HPLOT", "DRAW", "XDRAW", "HTAB", "HOME",
    "ROT=", "SCALE=", "SHLOAD", "TRACE", "NOTRACE", "NORMAL", "INVERSE", "FLASH",
    "COLOR=", "POP", "VTAB", "HIMEM:", "LOMEM:", "ONERR", "RESUME", "RECALL",
    "STORE", "SPEED=", "LET", "GOTO", "RUN", "IF", "RESTORE", "&",
    "GOSUB", "RETURN", "REM", "STOP", "ON", "WAIT", "LOAD", "SAVE",
    "DEF", "POKE", "PRINT", "CONT", "LIST", "CLEAR", "GET", "NEW",
    "TAB(", "TO", "FN", "SPC(", "THEN", "AT", "NOT", "STEP",
    "+", "-", "*", "/", "^", "AND", "OR", ">",
    "=", "<", "SGN", "INT", "ABS", "USR", "FRE", "SCRN(",
    "PDL", "POS", "SQR", "RND", "LOG", "EXP", "COS", "SIN",
    "TAN", "ATN", "PEEK", "LEN", "STR$", "VAL", "ASC", "CHR$",
    "LEFT$", "RIGHT$", "MID$"
};
#define NKEYWORDS ((int)(sizeof(keywords) / sizeof(keywords[0])))

const char *tok_name(unsigned char t)
{
    if (t < TOK_FIRST || t > TOK_LAST)
        return 0;
    return keywords[t - TOK_FIRST];
}

/* Does keyword k match src, ignoring spaces in src? The ROM skips spaces
 * while comparing, so "G O T O" is GOTO. Returns characters consumed, or 0. */
static int match_keyword(const char *src, const char *k)
{
    const char *s = src;
    while (*k) {
        while (*s == ' ')
            s++;
        if (toupper((unsigned char)*s) != *k)
            return 0;
        s++;
        k++;
    }
    return (int)(s - src);
}

static int is_ident_char(int c)
{
    return isalnum(c) || c == '$' || c == '%';
}

int tok_tokenize(const char *src, unsigned char *out, int outmax, int greedy)
{
    int n = 0;
    int prev_ident = 0;   /* previous emitted char was part of an identifier */

#define EMIT(b) do { if (n >= outmax - 1) return -1; out[n++] = (unsigned char)(b); } while (0)

    while (*src) {
        int i, matched = 0;

        if (*src == ' ') {           /* spaces outside strings are dropped */
            src++;
            /* A space breaks an identifier even though it is not stored, so
             * non-greedy mode still sees TO in "1 TO 10" as a keyword while
             * refusing the TO buried in TOTAL. */
            prev_ident = 0;
            continue;
        }

        {
            /* Numeric literals go through untouched. Scanning them here is
             * what makes 1E-5 work: leave it to the keyword matcher and the
             * "-" becomes a minus token, so the expression reads 1 - 5. */
            int nlen = mbf_literal_len(src);
            if (nlen) {
                while (nlen--)
                    EMIT(*src++);
                prev_ident = 1;
                continue;
            }
        }

        if (*src == '"') {           /* string literal: copy verbatim */
            EMIT(*src++);
            while (*src && *src != '"')
                EMIT(*src++);
            if (*src == '"')
                EMIT(*src++);
            prev_ident = 0;
            continue;
        }

        /* Keyword scan. Greedy mode matches anywhere, which is what turns
         * TOTAL into "TO TAL". Non-greedy mode additionally requires that an
         * alphabetic keyword not be welded to an identifier on either side,
         * so TOTAL survives -- at the cost of needing FOR I, not FORI.
         * Symbolic keywords (+, =, <) are never subject to that test. */
        /* Take the longest keyword that matches here, not the first one in
         * the table. AT comes before ATN, so first-match tokenizes ATN(1) as
         * AT followed by the variable N -- which is a syntax error, and which
         * LIST would show back as "AT N(1)". Longest-match does not soften
         * the greedy bug: TOTAL still becomes TO + TAL, because TOTAL is not
         * itself a keyword. */
        {
            int best = -1, best_used = 0;
            for (i = 0; i < NKEYWORDS; i++) {
                int used = match_keyword(src, keywords[i]);
                if (!used)
                    continue;
                if (!greedy && isalpha((unsigned char)keywords[i][0]) &&
                    (prev_ident || is_ident_char((unsigned char)src[used])))
                    continue;
                if (used > best_used) {
                    best_used = used;
                    best = i;
                }
            }
            if (best >= 0) {
                unsigned char t = (unsigned char)(TOK_FIRST + best);
                {
                    EMIT(t);
                    src += best_used;
                    matched = 1;
                    prev_ident = 0;

                    if (t == T_REM || t == T_LOAD || t == T_SAVE) {
                        /* REM keeps everything to the end of the line, and so
                         * do LOAD and SAVE: their argument is a DOS path, and
                         * tokenizing it would turn the slashes into division
                         * and eat keywords out of the directory names. */
                        while (*src)
                            EMIT(*src++);
                    } else if (t == T_DATA) {
                        /* DATA keeps its tail up to an unquoted colon. */
                        while (*src && *src != ':') {
                            if (*src == '"') {
                                EMIT(*src++);
                                while (*src && *src != '"')
                                    EMIT(*src++);
                                if (*src == '"')
                                    EMIT(*src++);
                            } else {
                                EMIT(*src++);
                            }
                        }
                    }
                }
            }
        }
        if (matched)
            continue;

        prev_ident = is_ident_char((unsigned char)*src);
        EMIT(toupper((unsigned char)*src));
        src++;
    }

    if (n >= outmax)
        return -1;
    out[n] = 0;
    return n;

#undef EMIT
}

/* Shared by both expanders. src_form drops the space a verbatim-tail keyword
 * would otherwise print, because that tail already begins with the space the
 * user typed: LIST shows "DATA  11" for a line entered as "DATA 11", so
 * writing the LIST form to a file and reading it back would gain a space on
 * every round trip. SAVE therefore writes the source form. */
static int expand(const unsigned char *toks, char *out, int outmax, int src_form)
{
    int n = 0;

#define PUT(c) do { if (n >= outmax - 1) { out[n] = '\0'; return n; } out[n++] = (char)(c); } while (0)

    while (*toks) {
        const char *k = tok_name(*toks);
        if (k) {
            int verbatim = (*toks == T_REM || *toks == T_DATA ||
                            *toks == T_LOAD || *toks == T_SAVE);
            /* " KEYWORD " - the space on each side is what makes LIST output
             * look the way it does. */
            PUT(' ');
            while (*k)
                PUT(*k++);
            if (!(src_form && verbatim))
                PUT(' ');
            if (verbatim) {
                /* Tail was stored verbatim; copy it out the same way. */
                toks++;
                while (*toks && !tok_name(*toks))
                    PUT(*toks++);
                continue;
            }
            toks++;
        } else if (*toks == '"') {
            PUT(*toks++);
            while (*toks && *toks != '"')
                PUT(*toks++);
            if (*toks == '"')
                PUT(*toks++);
        } else {
            PUT(*toks++);
        }
    }
    out[n] = '\0';
    return n;

#undef PUT
}

int tok_detokenize(const unsigned char *toks, char *out, int outmax)
{
    return expand(toks, out, outmax, 0);
}

int tok_detokenize_src(const unsigned char *toks, char *out, int outmax)
{
    return expand(toks, out, outmax, 1);
}
