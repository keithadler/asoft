#include "a2mem.h"
#include "mbf.h"
#include "errs.h"

#include <string.h>

unsigned char a2mem[A2_MEMSIZE];

unsigned char a2_peek(a2addr a) { return a2mem[a & 0xFFFF]; }
void a2_poke(a2addr a, unsigned char v) { a2mem[a & 0xFFFF] = v; }

a2addr a2_word(a2addr a)
{
    return (a2addr)(a2mem[a & 0xFFFF] | ((a2addr)a2mem[(a + 1) & 0xFFFF] << 8));
}

void a2_setword(a2addr a, a2addr v)
{
    a2mem[a & 0xFFFF] = (unsigned char)(v & 0xFF);
    a2mem[(a + 1) & 0xFFFF] = (unsigned char)((v >> 8) & 0xFF);
}

void a2_init(void)
{
    /* Cleared in two halves: the image is exactly 64K, and a 16-bit host
     * truncates a size of 65536 to zero, so a single memset would silently
     * clear nothing. */
    memset(a2mem, 0, A2_MEMSIZE / 2);
    memset(a2mem + A2_MEMSIZE / 2, 0, A2_MEMSIZE / 2);
    a2_setword(ZP_TXTTAB, A2_TXTTAB);
    a2_setword(ZP_MEMSIZ, A2_HIMEM);
    a2_new();
}

void a2_new(void)
{
    a2addr txt = a2_word(ZP_TXTTAB);
    /* An empty program is a zero next-pointer at TXTTAB. */
    a2_setword(txt, 0);
    a2_setword(ZP_VARTAB, txt + 2);
    a2_clear_vars();
}

void a2_clear_vars(void)
{
    a2addr vartab = a2_word(ZP_VARTAB);
    a2_setword(ZP_ARYTAB, vartab);
    a2_setword(ZP_STREND, vartab);
    a2_setword(ZP_FRETOP, a2_word(ZP_MEMSIZ));
}

/* --- program ------------------------------------------------------------ */

/* The program text ends two bytes below VARTAB, where the terminating zero
 * next-pointer lives. Deriving the end from VARTAB rather than by following
 * next-pointers means the walk still works while those pointers are stale,
 * which is exactly the situation relink() has to cope with. */
static a2addr prog_end(void);
a2addr a2_prog_end(void) { return prog_end(); }

static a2addr prog_end(void)
{
    return a2_word(ZP_VARTAB) - 2;
}

a2addr a2_prog_first(void)
{
    a2addr txt = a2_word(ZP_TXTTAB);
    return (prog_end() > txt) ? txt : 0;
}

a2addr a2_prog_next(a2addr line)
{
    a2addr nxt = a2_word(line);
    return (nxt && nxt < prog_end()) ? nxt : 0;
}

long a2_prog_lineno(a2addr line)
{
    return (long)a2_word(line + 2);
}

const unsigned char *a2_prog_tokens(a2addr line)
{
    return &a2mem[(line + 4) & 0xFFFF];
}

a2addr a2_prog_find(long lineno)
{
    a2addr p;
    for (p = a2_prog_first(); p; p = a2_prog_next(p))
        if (a2_prog_lineno(p) == lineno)
            return p;
    return 0;
}

a2addr a2_prog_find_ge(long lineno)
{
    a2addr p;
    for (p = a2_prog_first(); p; p = a2_prog_next(p))
        if (a2_prog_lineno(p) >= lineno)
            return p;
    return 0;
}

/* Rewrite every next-line pointer after an insert or delete. Walks by line
 * length, not by the pointers it is about to overwrite. */
static void relink(void)
{
    a2addr end = prog_end();
    a2addr p = a2_word(ZP_TXTTAB);
    while (p < end) {
        a2addr len = 4;
        while (a2mem[(p + len) & 0xFFFF])
            len++;
        len++;                       /* the terminating 0 */
        a2_setword(p, p + len);
        p = p + len;
    }
    a2_setword(end, 0);
}

void a2_prog_delete(long lineno)
{
    a2addr line = a2_prog_find(lineno);
    a2addr nxt, end, vartab;
    long shift;

    if (!line)
        return;
    nxt = a2_word(line);
    end = prog_end();
    shift = (long)nxt - (long)line;

    memmove(&a2mem[line], &a2mem[nxt], (size_t)(end + 2 - nxt));

    vartab = a2_word(ZP_VARTAB);
    a2_setword(ZP_VARTAB, (a2addr)(vartab - shift));
    relink();
    a2_clear_vars();
}

int a2_prog_insert(long lineno, const unsigned char *toks, int len)
{
    a2addr at, end, vartab;
    int need;

    a2_prog_delete(lineno);
    if (len == 0)
        return 1;                    /* "10" on its own just deletes line 10 */

    need = 4 + len + 1;
    end = prog_end();
    if (end + 2 + need >= a2_word(ZP_FRETOP))
        return 0;                    /* out of memory */

    /* Lines are kept sorted, so find the first line with a larger number. */
    at = a2_prog_find_ge(lineno + 1);
    if (!at)
        at = end;

    memmove(&a2mem[at + need], &a2mem[at], (size_t)(end + 2 - at));
    a2_setword(at, 0);
    a2_setword(at + 2, (a2addr)lineno);
    memcpy(&a2mem[at + 4], toks, (size_t)len);
    a2mem[at + 4 + len] = 0;

    vartab = a2_word(ZP_VARTAB);
    a2_setword(ZP_VARTAB, vartab + need);
    relink();
    a2_clear_vars();
    return 1;
}

/* --- variables ---------------------------------------------------------- */

/* Applesoft packs the type into the high bits of the two name bytes:
 * real clears both, string sets bit 7 of the second, integer sets both. */
static void name_bytes(const char *name, int type, unsigned char *b)
{
    b[0] = (unsigned char)name[0];
    b[1] = (unsigned char)(name[1] ? name[1] : 0);
    if (type == VT_STR)
        b[1] |= 0x80;
    else if (type == VT_INT) {
        b[0] |= 0x80;
        b[1] |= 0x80;
    }
}

a2addr a2_var(const char *name, int type, int create)
{
    unsigned char want[2];
    a2addr p, vartab, arytab, strend;

    name_bytes(name, type, want);
    vartab = a2_word(ZP_VARTAB);
    arytab = a2_word(ZP_ARYTAB);

    for (p = vartab; p < arytab; p += 7)
        if (a2mem[p] == want[0] && a2mem[p + 1] == want[1])
            return p + 2;

    if (!create)
        return 0;

    /* New scalars go at ARYTAB; the arrays above shuffle up by 7. */
    strend = a2_word(ZP_STREND);
    if (strend + 7 >= a2_word(ZP_FRETOP))
        return 0;

    memmove(&a2mem[arytab + 7], &a2mem[arytab], (size_t)(strend - arytab));
    a2mem[arytab] = want[0];
    a2mem[arytab + 1] = want[1];
    memset(&a2mem[arytab + 2], 0, 5);

    a2_setword(ZP_ARYTAB, arytab + 7);
    a2_setword(ZP_STREND, strend + 7);
    return arytab + 2;
}

/* Array block layout: name (2), block length (2), ndims (1), then each
 * dimension's element count (2 each, first dimension first), then the data. */
int a2_array_exists(const char *name, int type)
{
    unsigned char want[2];
    a2addr p, arytab, strend;

    name_bytes(name, type, want);
    arytab = a2_word(ZP_ARYTAB);
    strend = a2_word(ZP_STREND);
    for (p = arytab; p < strend; p += a2_word(p + 2))
        if (a2mem[p] == want[0] && a2mem[p + 1] == want[1])
            return 1;
    return 0;
}

/* Bytes per element: a real is its five-byte float, a string its three-byte
 * descriptor, an integer its two bytes. */
static int elem_size(int type)
{
    return type == VT_STR ? 3 : type == VT_INT ? 2 : 5;
}

/* Create an array with the given highest subscript in each dimension: what
 * DIM does with the sizes it was given, and what first use does with ten. */
a2addr a2_array_dim(const char *name, int type, const int *dims, int ndims,
                    int *err)
{
    unsigned char want[2];
    a2addr p, strend = a2_word(ZP_STREND);
    long count = 1;
    int hdr = 5 + 2 * ndims;
    long bytes;
    int i;

    *err = 0;
    name_bytes(name, type, want);
    for (i = 0; i < ndims; i++) {
        if (dims[i] < 0) { *err = ERR_BADSUBSCRIPT; return 0; }
        count *= (long)dims[i] + 1;
    }
    bytes = hdr + count * elem_size(type);
    if (strend + bytes >= (long)a2_word(ZP_FRETOP)) {
        *err = ERR_OUTOFMEM;
        return 0;
    }
    p = strend;
    a2mem[p] = want[0];
    a2mem[p + 1] = want[1];
    a2_setword(p + 2, (a2addr)bytes);
    a2mem[p + 4] = (unsigned char)ndims;
    for (i = 0; i < ndims; i++)
        a2_setword(p + 5 + 2 * i, (a2addr)(dims[i] + 1));
    memset(&a2mem[p + hdr], 0, (size_t)(count * elem_size(type)));
    a2_setword(ZP_STREND, (a2addr)(strend + bytes));
    return p;
}

a2addr a2_array(const char *name, int type, const int *idx, int ndims,
                int create, int *err)
{
    unsigned char want[2];
    a2addr p, arytab, strend;
    int i;

    *err = 0;
    name_bytes(name, type, want);
    arytab = a2_word(ZP_ARYTAB);
    strend = a2_word(ZP_STREND);

    for (p = arytab; p < strend; p += a2_word(p + 2)) {
        if (a2mem[p] == want[0] && a2mem[p + 1] == want[1]) {
            int nd = a2mem[p + 4];
            long off = 0;
            if (nd != ndims) {
                *err = ERR_BADSUBSCRIPT;
                return 0;
            }
            /* Row-major, first subscript varying slowest. */
            for (i = 0; i < nd; i++) {
                int size = (int)a2_word(p + 5 + 2 * i);
                if (idx[i] < 0 || idx[i] >= size) {
                    *err = ERR_BADSUBSCRIPT;
                    return 0;
                }
                off = off * size + idx[i];
            }
            return (a2addr)(p + 5 + 2 * nd + off * elem_size(type));
        }
    }

    if (!create) {
        *err = ERR_BADSUBSCRIPT;
        return 0;
    }

    /* An array used before it is dimensioned gets 0..10 in every subscript,
     * as the ROM does -- not the subscript it was first used with, which is
     * what a program doing F(1,1)=0 then F(1,2)=0 relies on. */
    {
        int dims[8];
        for (i = 0; i < ndims && i < 8; i++)
            dims[i] = 10;
        if (!a2_array_dim(name, type, dims, ndims, err))
            return 0;
        /* Recurse now that the block exists. */
        return a2_array(name, type, idx, ndims, 0, err);
    }
}

/* --- strings ------------------------------------------------------------ */

void a2_str_get(a2addr slot, a2str *s)
{
    s->len = a2mem[slot];
    s->addr = a2_word(slot + 1);
}

void a2_str_put(a2addr slot, const a2str *s)
{
    /* Three bytes, whether the slot is a scalar's five or an array's
     * three: a string array packs its descriptors without a gap. */
    a2mem[slot] = s->len;
    a2_setword(slot + 1, s->addr);
}

a2addr a2_str_alloc(int len)
{
    a2addr fretop = a2_word(ZP_FRETOP);
    if (len == 0)
        return fretop;
    if ((long)fretop - len <= (long)a2_word(ZP_STREND)) {
        a2_gc();
        fretop = a2_word(ZP_FRETOP);
        if ((long)fretop - len <= (long)a2_word(ZP_STREND))
            return 0;
    }
    fretop -= len;
    a2_setword(ZP_FRETOP, fretop);
    return fretop;
}

/* Compact string space by copying every live string to the top, highest
 * address first. Only strings living above STREND move; descriptors pointing
 * into program text are left alone. */
void a2_gc(void)
{
    a2addr himem = a2_word(ZP_MEMSIZ);
    a2addr strend = a2_word(ZP_STREND);
    a2addr dest = himem;

    for (;;) {
        /* Find the live string with the highest address below dest. */
        a2addr best = 0, best_slot = 0;
        a2addr p, arytab, vartab, strend2;

        vartab = a2_word(ZP_VARTAB);
        arytab = a2_word(ZP_ARYTAB);
        strend2 = a2_word(ZP_STREND);

        for (p = vartab; p < arytab; p += 7) {
            if ((a2mem[p + 1] & 0x80) && !(a2mem[p] & 0x80)) {
                a2addr a = a2_word(p + 3);
                if (a2mem[p + 2] && a > strend2 && a < dest && a >= best) {
                    best = a;
                    best_slot = p + 2;
                }
            }
        }
        for (p = arytab; p < strend2; p += a2_word(p + 2)) {
            if ((a2mem[p + 1] & 0x80) && !(a2mem[p] & 0x80)) {
                int nd = a2mem[p + 4];
                a2addr d = p + 5 + 2 * nd;
                a2addr endb = p + a2_word(p + 2);
                for (; d < endb; d += 3) {
                    a2addr a = a2_word(d + 1);
                    if (a2mem[d] && a > strend2 && a < dest && a >= best) {
                        best = a;
                        best_slot = d;
                    }
                }
            }
        }
        if (!best_slot)
            break;

        {
            int len = a2mem[best_slot];
            a2addr to = dest - len;
            memmove(&a2mem[to], &a2mem[best], (size_t)len);
            a2_setword(best_slot + 1, to);
            dest = to;
        }
    }

    if (dest < strend)
        dest = strend;
    a2_setword(ZP_FRETOP, dest);
}

long a2_fre(void)
{
    a2_gc();
    return (long)a2_word(ZP_FRETOP) - (long)a2_word(ZP_STREND);
}

/* --- introspection ------------------------------------------------------ */

int a2_var_count(void)
{
    a2addr vartab = a2_word(ZP_VARTAB);
    a2addr arytab = a2_word(ZP_ARYTAB);
    return (int)((arytab - vartab) / 7);
}

int a2_var_info(int i, char *name, int *type, a2addr *slot)
{
    a2addr p = a2_word(ZP_VARTAB) + (a2addr)i * 7;
    unsigned char b0, b1;

    if (i < 0 || i >= a2_var_count())
        return 0;
    b0 = a2mem[p];
    b1 = a2mem[p + 1];

    if ((b0 & 0x80) && (b1 & 0x80))
        *type = VT_INT;
    else if (b1 & 0x80)
        *type = VT_STR;
    else
        *type = VT_REAL;

    name[0] = (char)(b0 & 0x7F);
    name[1] = (char)(b1 & 0x7F);
    name[2] = '\0';
    if (name[1] == '\0')
        name[1] = '\0';
    *slot = p + 2;
    return 1;
}
