/* token.h - the Applesoft keyword table, and the two conversions around it.
 *
 * A stored program line is a run of bytes where anything >= 0x80 is a keyword
 * and anything else is a literal character. Tokenising is greedy and
 * position-independent: the scanner tries every keyword at every character
 * outside a string, which is why TOTAL = 5 becomes "TO TAL = 5" and fails.
 * That is ROM behaviour, not an accident, and BUG_GREEDY_TOKENIZER exists to
 * turn it off for anyone who wants to see what breaks.
 *
 * Spaces outside strings are discarded while tokenising. LIST puts them back
 * by printing every keyword as " KEYWORD ", which is where Applesoft's
 * distinctive double spaces come from: the line number contributes one space
 * and the first keyword contributes another, giving "10  PRINT". Two
 * statements keep their tails verbatim instead -- REM to the end of the line,
 * DATA to the next unquoted colon -- so "10 REM HI" lists as "10  REM  HI".
 */
#ifndef ASOFT_TOKEN_H
#define ASOFT_TOKEN_H

#define TOK_FIRST 0x80

enum {
    T_END = 0x80, T_FOR, T_NEXT, T_DATA, T_INPUT, T_DEL, T_DIM, T_READ,
    T_GR, T_TEXT, T_PRNUM, T_INNUM, T_CALL, T_PLOT, T_HLIN, T_VLIN,
    T_HGR2, T_HGR, T_HCOLOR, T_HPLOT, T_DRAW, T_XDRAW, T_HTAB, T_HOME,
    T_ROT, T_SCALE, T_SHLOAD, T_TRACE, T_NOTRACE, T_NORMAL, T_INVERSE, T_FLASH,
    T_COLOR, T_POP, T_VTAB, T_HIMEM, T_LOMEM, T_ONERR, T_RESUME, T_RECALL,
    T_STORE, T_SPEED, T_LET, T_GOTO, T_RUN, T_IF, T_RESTORE, T_AMP,
    T_GOSUB, T_RETURN, T_REM, T_STOP, T_ON, T_WAIT, T_LOAD, T_SAVE,
    T_DEF, T_POKE, T_PRINT, T_CONT, T_LIST, T_CLEAR, T_GET, T_NEW,
    T_TABPAREN, T_TO, T_FN, T_SPCPAREN, T_THEN, T_AT, T_NOT, T_STEP,
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_CARET, T_AND, T_OR, T_GT,
    T_EQ, T_LT, T_SGN, T_INT, T_ABS, T_USR, T_FRE, T_SCRNPAREN,
    T_PDL, T_POS, T_SQR, T_RND, T_LOG, T_EXP, T_COS, T_SIN,
    T_TAN, T_ATN, T_PEEK, T_LEN, T_STR, T_VAL, T_ASC, T_CHR,
    T_LEFT, T_RIGHT, T_MID
};
#define TOK_LAST T_MID

/* Text of a token, or NULL if t is not one. */
const char *tok_name(unsigned char t);

/* Tokenise one line of source into out. Returns the number of bytes written
 * (not counting the terminating 0, which is written) or -1 if it will not fit.
 * greedy selects ROM keyword matching; when 0, keywords are only recognised at
 * the start of an identifier, which makes TOTAL a usable variable name. */
int tok_tokenize(const char *src, unsigned char *out, int outmax, int greedy);

/* Expand tokens back to text the way LIST does. Returns bytes written. */
int tok_detokenize(const unsigned char *toks, char *out, int outmax);

#endif
