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
a2addr a2_prog_first(void);                    /* 0 when empty */
a2addr a2_prog_next(a2addr line);              /* 0 at end */
int    a2_prog_lineno(a2addr line);
const unsigned char *a2_prog_tokens(a2addr line);
a2addr a2_prog_find(int lineno);               /* 0 if absent */
a2addr a2_prog_find_ge(int lineno);            /* first line >= lineno */
int    a2_prog_insert(int lineno, const unsigned char *toks, int len);
void   a2_prog_delete(int lineno);

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
