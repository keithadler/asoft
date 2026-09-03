/* a2mem.h - the 64K memory image Applesoft lives inside.
 *
 * Keeping a real memory image rather than C structs is what makes PEEK, POKE,
 * FRE and the zero-page pointers mean anything, and it is what the Machine
 * pane in the Turbo Vision front end reads. The layout is Applesoft's:
 *
 *   $0801  TXTTAB   tokenised program, lines in ascending order
 *          VARTAB   scalar variables, 7 bytes each
 *          ARYTAB   arrays
 *          STREND   first free byte
 *            ...    free space
 *          FRETOP   bottom of string space, grows downward
 *   $9600  HIMEM
 *
 * FRE(0) is FRETOP - STREND, after a garbage collection. Each program line is
 * stored as: next-line pointer (2), line number (2), tokens, 0. A zero next
 * pointer ends the program.
 */
#ifndef ASOFT_A2MEM_H
#define ASOFT_A2MEM_H

#define A2_MEMSIZE  0x10000
#define A2_TXTTAB   0x0801
#define A2_HIMEM    0x9600

/* Zero page, at the addresses the ROM uses, so POKE 216,0 and CALL -3288 in
 * ONERRFIX.BAS reach the things they are supposed to reach. */
#define ZP_TXTTAB   0x67
#define ZP_VARTAB   0x69
#define ZP_ARYTAB   0x6B
#define ZP_STREND   0x6D
#define ZP_FRETOP   0x6F
#define ZP_MEMSIZ   0x73
#define ZP_CURLIN   0x75
/* Soft switches. $C000 holds the last key with bit 7 set while it is still
 * unread; reading or writing $C010 clears that bit. Games poll these rather
 * than calling GET, because GET stops until something is typed. */
#define KBD_DATA    0xC000
#define KBD_STROBE  0xC010

/* The text window and cursor, where the ROM kept them and where programs
 * POKEd them: POKE 34,20 for a status line, POKE 33,33 to stop INPUT
 * wrapping, PEEK(36) for the column. screen.c keeps its state here. */
#define ZP_WNDLFT   0x20          /* left edge of the text window */
#define ZP_WNDWDTH  0x21          /* its width */
#define ZP_WNDTOP   0x22          /* first row */
#define ZP_WNDBTM   0x23          /* one past the last row */
#define ZP_CH       0x24          /* cursor column, within the window */
#define ZP_CV       0x25          /* cursor row, absolute */
#define ZP_HCOLOR1  0x1C          /* the colour byte HPLOT last used, aligned to its byte */
#define ZP_COLOR    0x30          /* COLOR= n, stored as n * 17 */
#define ZP_INVFLG   0x32          /* $FF normal, $3F inverse, $7F flash */
#define ZP_PROMPT   0x33          /* the prompt character, "]" */
#define ZP_RUNFLAG  0xD6          /* bit 7 set: every command is RUN */
#define ZP_HCOLOR   0xE4          /* HCOLOR= as the byte pattern it plots */
#define ZP_HPAG     0xE6          /* high byte of the page HPLOT draws on */
#define SPKR        0xC030        /* the speaker: any access clicks it */

#define ZP_SCALE    0xE7          /* SCALE= */
#define ZP_SHAPE    0xE8          /* pointer to the shape table */
#define ZP_COLLISION 0xEA         /* bumped whenever DRAW hits a lit pixel */
#define ZP_ROT      0xF9          /* ROT= */
#define ZP_ONERRFLAG 0xD8
#define ZP_ERRLIN   0xDA
#define ZP_ERRNUM   0xDE

typedef unsigned int a2addr;

extern unsigned char a2mem[A2_MEMSIZE];

/* A string as Applesoft sees one: a length and a pointer into the image.
 * The bytes may sit in program text (for a literal) or in string space. */
typedef struct {
    unsigned char len;
    a2addr addr;
} a2str;

void   a2_init(void);
void   a2_new(void);         /* wipe program and variables */
void   a2_clear_vars(void);  /* CLEAR: variables only */

unsigned char a2_peek(a2addr a);
void   a2_poke(a2addr a, unsigned char v);
a2addr a2_word(a2addr a);              /* little-endian 16-bit read */
void   a2_setword(a2addr a, a2addr v);

/* --- program ------------------------------------------------------------ */
/* Line numbers are long throughout. Applesoft allows up to 63999, which does
 * not fit in the 16-bit int of the DOS build -- LIST's upper bound of 65535
 * silently became -1 and listed nothing until this was widened. */
a2addr a2_prog_first(void);                    /* 0 when empty */
a2addr a2_prog_end(void);                      /* first byte past the program */
a2addr a2_prog_next(a2addr line);              /* 0 at end */
long   a2_prog_lineno(a2addr line);
const unsigned char *a2_prog_tokens(a2addr line);
a2addr a2_prog_find(long lineno);              /* 0 if absent */
a2addr a2_prog_find_ge(long lineno);           /* first line >= lineno */
int    a2_prog_insert(long lineno, const unsigned char *toks, int len);
void   a2_prog_delete(long lineno);

/* --- variables ---------------------------------------------------------- */
#define VT_REAL 0
#define VT_STR  1
#define VT_INT  2

/* Address of a scalar's 5 value bytes, creating it if asked. 0 when absent
 * and create is 0. name is up to two significant characters, upper case. */
a2addr a2_var(const char *name, int type, int create);

/* Address of one array element's value bytes. idx holds ndims subscripts.
 * err is set to an ERR_* code on failure (bad subscript, redim). */
a2addr a2_array(const char *name, int type, const int *idx, int ndims,
                int create, int *err);

/* DIM: create it with these highest subscripts. Fails with ERR_OUTOFMEM. */
a2addr a2_array_dim(const char *name, int type, const int *dims, int ndims,
                    int *err);

/* Has this array been created yet, by DIM or by being used? DIM needs to
 * know, because dimensioning one twice is an error rather than a resize. */
int a2_array_exists(const char *name, int type);

/* --- strings ------------------------------------------------------------ */
/* Reserve len bytes at the bottom of string space. Returns 0 if there is no
 * room even after a collection. */
a2addr a2_str_alloc(int len);
void   a2_gc(void);
long   a2_fre(void);

/* Read a descriptor out of a variable slot, and write one in. */
void   a2_str_get(a2addr slot, a2str *s);
void   a2_str_put(a2addr slot, const a2str *s);

/* Iteration for the Machine pane / debugger. */
int    a2_var_count(void);
int    a2_var_info(int i, char *name, int *type, a2addr *slot);

#endif
