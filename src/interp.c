#include "interp.h"
#include "a2mem.h"
#include "bugs.h"
#include "errs.h"
#include "gfx.h"
#include "host.h"
#include "mbf.h"
#include "screen.h"
#include "token.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* ------------------------------------------------------------------ state */

typedef struct {
    int   is_str;
    double num;
    a2str str;
} value;

typedef struct {
    int    kind;
    int    bytes;
    /* FOR */
    a2addr var;
    double limit, step;
    /* resume point, for FOR and GOSUB alike */
    a2addr line;
    int    offset;
} frame;

static frame cstack[CSTACK_BYTES / 4];
static int   ncstack;
static int   cstack_bytes;
static int   leaked;              /* ONERR frames the leak has stranded */

static a2addr cur_line;           /* 0 while in immediate mode */
static const unsigned char *ip;
static unsigned char imm[512];    /* tokenised immediate-mode line */

static int   running;
static int   quitting;
static int   stopped_line;        /* where STOP left off, for CONT */
static int   stopped_offset;

static a2addr data_line;
static int    data_offset;

static int   onerr_line;
static int   last_error = ERR_NONE;
static int   last_error_line;

static jmp_buf err_jmp;

/* User-defined functions. The body is copied rather than referenced so that
 * DEF FN typed at the prompt does not leave a pointer into a dead buffer. */
#define MAXFN 16
static struct {
    char name[3];
    char param[3];
    unsigned char body[128];
    int  used;
} fns[MAXFN];

static void raise_err(int code)
{
    longjmp(err_jmp, code + 1);       /* +1 so code 0 is distinguishable */
}

/* --------------------------------------------------------- token cursor */

static int eat(unsigned char t)
{
    if (*ip == t) { ip++; return 1; }
    return 0;
}

static void expect(unsigned char t)
{
    if (!eat(t))
        raise_err(ERR_SYNTAX);
}

static void skip_to_eos(void)
{
    /* End of statement: a colon or the end of the line. */
    while (*ip && *ip != ':')
        ip++;
}

/* ------------------------------------------------------------- strings */

static a2str str_make(const char *bytes, int len)
{
    a2str s;
    a2addr a;
    if (len > 255)
        raise_err(ERR_STRINGTOOLONG);
    s.len = (unsigned char)len;
    if (len == 0) { s.addr = a2_word(ZP_FRETOP); return s; }
    a = a2_str_alloc(len);
    if (!a)
        raise_err(ERR_OUTOFMEM);
    memcpy(&a2mem[a], bytes, (size_t)len);
    s.addr = a;
    return s;
}

/* Copy a string out of the memory image into a C buffer. */
static void str_read(const a2str *s, char *buf)
{
    memcpy(buf, &a2mem[s->addr], (size_t)s->len);
    buf[s->len] = '\0';
}

static int str_cmp(const a2str *a, const a2str *b)
{
    int n = a->len < b->len ? a->len : b->len;
    int c = n ? memcmp(&a2mem[a->addr], &a2mem[b->addr], (size_t)n) : 0;
    if (c)
        return c;
    return (int)a->len - (int)b->len;
}

/* --------------------------------------------------------- number store */

static double num_round(double v)
{
    return bug_enabled[BUG_MBF_ROUNDING] ? mbf_round(v) : v;
}

static void store_num(a2addr slot, double v)
{
    mbf_t m;
    if (!mbf_pack(num_round(v), &m))
        raise_err(ERR_OVERFLOW);
    memcpy(&a2mem[slot], m.b, MBF_SIZE);
}

static double load_num(a2addr slot)
{
    mbf_t m;
    memcpy(m.b, &a2mem[slot], MBF_SIZE);
    return mbf_unpack(&m);
}

/* ------------------------------------------------------------ variables */

/* Read a variable name at ip. Applesoft keeps two significant characters and
 * ignores the rest, so LENGTH and LENIENT are the same variable. */
static int read_name(char *name, int *type)
{
    int n = 0;
    if (!isalpha((unsigned char)*ip))
        return 0;
    while (isalnum((unsigned char)*ip)) {
        if (n < 2)
            name[n++] = (char)toupper((unsigned char)*ip);
        ip++;
    }
    name[n] = '\0';
    if (n == 1)
        name[1] = '\0';
    *type = VT_REAL;
    if (*ip == '$') { *type = VT_STR; ip++; }
    else if (*ip == '%') { *type = VT_INT; ip++; }
    return 1;
}

static void eval(value *out);

/* Locate the slot a variable reference at ip designates, creating it if
 * asked. Handles both scalars and array elements. */
static a2addr lvalue(int *type)
{
    char name[3];
    if (!read_name(name, type))
        raise_err(ERR_SYNTAX);

    if (*ip == '(') {
        int idx[4], nd = 0, err;
        a2addr slot;
        ip++;
        do {
            value v;
            eval(&v);
            if (v.is_str)
                raise_err(ERR_TYPEMISMATCH);
            if (nd >= 4)
                raise_err(ERR_BADSUBSCRIPT);
            if (v.num < 0 || v.num > 32767)
                raise_err(ERR_BADSUBSCRIPT);
            idx[nd++] = (int)v.num;
        } while (eat(','));
        expect(')');
        slot = a2_array(name, *type, idx, nd, 1, &err);
        if (!slot)
            raise_err(err ? err : ERR_BADSUBSCRIPT);
        return slot;
    }

    {
        a2addr slot = a2_var(name, *type, 1);
        if (!slot)
            raise_err(ERR_OUTOFMEM);
        return slot;
    }
}

static void load_value(a2addr slot, int type, value *out)
{
    out->is_str = (type == VT_STR);
    if (out->is_str)
        a2_str_get(slot, &out->str);
    else
        out->num = load_num(slot);
}

static void store_value(a2addr slot, int type, const value *v)
{
    if ((type == VT_STR) != v->is_str)
        raise_err(ERR_TYPEMISMATCH);
    if (type == VT_STR)
        a2_str_put(slot, &v->str);
    else if (type == VT_INT)
        store_num(slot, (double)(long)v->num);
    else
        store_num(slot, v->num);
}

/* ----------------------------------------------------------- functions */

static void call_builtin(unsigned char t, value *out);
static void call_user_fn(value *out);

static double need_num(void)
{
    value v;
    eval(&v);
    if (v.is_str)
        raise_err(ERR_TYPEMISMATCH);
    return v.num;
}

static double paren_num(void)
{
    double v;
    expect('(');
    v = need_num();
    expect(')');
    return v;
}

static void primary(value *out)
{
    if (*ip == '(') {
        ip++;
        eval(out);
        expect(')');
        return;
    }

    if (*ip == '"') {
        /* A literal in program text is not copied: the descriptor points at
         * the program itself, which is why A$ = "HELLO" costs no string
         * space. The reference build does the same. */
        const unsigned char *start = ++ip;
        int len = 0;
        while (ip[len] && ip[len] != '"')
            len++;
        out->is_str = 1;
        out->str.len = (unsigned char)len;
        out->str.addr = (a2addr)(start - a2mem);
        ip += len;
        if (*ip == '"')
            ip++;
        /* An immediate-mode line lives outside the image, so it has to be
         * copied to somewhere the descriptor can point at. */
        if (start < a2mem || start >= a2mem + A2_MEMSIZE)
            out->str = str_make((const char *)start, len);
        return;
    }

    if (isdigit((unsigned char)*ip) || *ip == '.') {
        double v;
        int n = mbf_parse((const char *)ip, &v);
        if (!n)
            raise_err(ERR_SYNTAX);
        ip += n;
        out->is_str = 0;
        out->num = v;
        return;
    }

    if (*ip == T_FN) {
        ip++;
        call_user_fn(out);
        return;
    }

    if (*ip >= T_SGN && *ip <= T_MID) {
        unsigned char t = *ip++;
        call_builtin(t, out);
        return;
    }

    if (isalpha((unsigned char)*ip)) {
        int type;
        a2addr slot;
        const unsigned char *save = ip;
        char name[3];
        int t2;
        /* Reading an undefined variable must not create an array by
         * accident, but Applesoft does create scalars on read, initialised
         * to zero or the empty string. */
        (void)save; (void)name; (void)t2;
        slot = lvalue(&type);
        load_value(slot, type, out);
        return;
    }

    raise_err(ERR_SYNTAX);
}

static void unary_expr(value *out)
{
    if (eat(T_MINUS)) {
        unary_expr(out);
        if (out->is_str)
            raise_err(ERR_TYPEMISMATCH);
        out->num = -out->num;
        return;
    }
    if (eat(T_PLUS)) {
        unary_expr(out);
        return;
    }
    primary(out);
}

static void pow_expr(value *out)
{
    unary_expr(out);
    if (eat(T_CARET)) {
        value rhs;
        pow_expr(&rhs);              /* right associative */
        if (out->is_str || rhs.is_str)
            raise_err(ERR_TYPEMISMATCH);
        if (out->num < 0 && rhs.num != floor(rhs.num))
            raise_err(ERR_ILLEGALQTY);
        out->num = num_round(pow(out->num, rhs.num));
    }
}

static void mul_expr(value *out)
{
    pow_expr(out);
    for (;;) {
        int div = 0;
        if (eat(T_STAR)) div = 0;
        else if (eat(T_SLASH)) div = 1;
        else return;
        {
            value rhs;
            pow_expr(&rhs);
            if (out->is_str || rhs.is_str)
                raise_err(ERR_TYPEMISMATCH);
            if (div) {
                if (rhs.num == 0.0)
                    raise_err(ERR_DIVBYZERO);
                out->num = num_round(out->num / rhs.num);
            } else {
                out->num = num_round(out->num * rhs.num);
            }
        }
    }
}

static void add_expr(value *out)
{
    mul_expr(out);
    for (;;) {
        int sub;
        if (eat(T_PLUS)) sub = 0;
        else if (eat(T_MINUS)) sub = 1;
        else return;
        {
            value rhs;
            mul_expr(&rhs);
            if (out->is_str || rhs.is_str) {
                char a[256], b[256];
                if (sub || !out->is_str || !rhs.is_str)
                    raise_err(ERR_TYPEMISMATCH);
                if ((int)out->str.len + (int)rhs.str.len > 255)
                    raise_err(ERR_STRINGTOOLONG);
                str_read(&out->str, a);
                str_read(&rhs.str, b);
                strcat(a, b);
                out->str = str_make(a, (int)strlen(a));
                continue;
            }
            out->num = num_round(sub ? out->num - rhs.num : out->num + rhs.num);
        }
    }
}

static void rel_expr(value *out)
{
    add_expr(out);
    for (;;) {
        int gt = 0, eq = 0, lt = 0, any = 0;
        /* Applesoft stores <= as two tokens, in whatever order they were
         * typed, so accept any run of <, = and >. */
        for (;;) {
            if (eat(T_GT)) { gt = 1; any = 1; continue; }
            if (eat(T_EQ)) { eq = 1; any = 1; continue; }
            if (eat(T_LT)) { lt = 1; any = 1; continue; }
            break;
        }
        if (!any)
            return;
        {
            value rhs;
            int c;
            add_expr(&rhs);
            if (out->is_str != rhs.is_str)
                raise_err(ERR_TYPEMISMATCH);
            if (out->is_str)
                c = str_cmp(&out->str, &rhs.str);
            else
                c = (out->num < rhs.num) ? -1 : (out->num > rhs.num) ? 1 : 0;
            out->is_str = 0;
            out->num = ((c < 0 && lt) || (c == 0 && eq) || (c > 0 && gt)) ? 1.0 : 0.0;
        }
    }
}

static void not_expr(value *out)
{
    if (eat(T_NOT)) {
        not_expr(out);
        if (out->is_str)
            raise_err(ERR_TYPEMISMATCH);
        out->num = (out->num == 0.0) ? 1.0 : 0.0;
        return;
    }
    rel_expr(out);
}

static void and_expr(value *out)
{
    not_expr(out);
    while (eat(T_AND)) {
        value rhs;
        not_expr(&rhs);
        if (out->is_str || rhs.is_str)
            raise_err(ERR_TYPEMISMATCH);
        out->num = (out->num != 0.0 && rhs.num != 0.0) ? 1.0 : 0.0;
    }
}

static void eval(value *out)
{
    and_expr(out);
    while (eat(T_OR)) {
        value rhs;
        and_expr(&rhs);
        if (out->is_str || rhs.is_str)
            raise_err(ERR_TYPEMISMATCH);
        out->num = (out->num != 0.0 || rhs.num != 0.0) ? 1.0 : 0.0;
    }
}

/* ------------------------------------------------------------- builtins */

static unsigned long rnd_state = 1;
static double rnd_last = 0.0;

static void call_builtin(unsigned char t, value *out)
{
    out->is_str = 0;

    switch (t) {
    case T_SGN: { double v = paren_num(); out->num = (v > 0) - (v < 0); return; }
    case T_INT: out->num = floor(paren_num()); return;
    case T_ABS: out->num = fabs(paren_num()); return;
    case T_USR: (void)paren_num(); out->num = 0; return;
    case T_FRE: (void)paren_num(); out->num = (double)a2_fre(); return;
    case T_PDL: (void)paren_num(); out->num = 0; return;
    case T_POS: (void)paren_num(); out->num = scr_col(); return;

    case T_SCRNPAREN: {              /* the "(" is part of the token */
        double x = need_num(), y;
        expect(',');
        y = need_num();
        expect(')');
        if (x < 0 || x > 39 || y < 0 || y > 47)
            raise_err(ERR_ILLEGALQTY);
        out->num = gfx_scrn((int)x, (int)y);
        return;
    }

    case T_SQR: {
        double v = paren_num();
        if (v < 0) raise_err(ERR_ILLEGALQTY);
        out->num = num_round(sqrt(v));
        return;
    }
    case T_LOG: {
        double v = paren_num();
        if (v <= 0) raise_err(ERR_ILLEGALQTY);
        out->num = num_round(log(v));
        return;
    }
    case T_EXP: out->num = num_round(exp(paren_num())); return;
    case T_COS: out->num = num_round(cos(paren_num())); return;
    case T_SIN: out->num = num_round(sin(paren_num())); return;
    case T_TAN: out->num = num_round(tan(paren_num())); return;
    case T_ATN: out->num = num_round(atan(paren_num())); return;

    case T_RND: {
        double v = paren_num();
        if (v == 0.0) { out->num = rnd_last; return; }
        if (v < 0.0) rnd_state = (unsigned long)(-v);
        rnd_state = rnd_state * 1103515245UL + 12345UL;
        rnd_last = (double)((rnd_state >> 16) & 0x7FFF) / 32768.0;
        out->num = rnd_last;
        return;
    }

    case T_PEEK: {
        double v = paren_num();
        if (v < -65535.0 || v > 65535.0) raise_err(ERR_ILLEGALQTY);
        out->num = a2_peek((a2addr)((long)v & 0xFFFF));
        return;
    }

    case T_LEN: {
        value s;
        expect('(');
        eval(&s);
        expect(')');
        if (!s.is_str) raise_err(ERR_TYPEMISMATCH);
        out->num = s.str.len;
        return;
    }

    case T_ASC: {
        value s;
        expect('(');
        eval(&s);
        expect(')');
        if (!s.is_str) raise_err(ERR_TYPEMISMATCH);
        if (s.str.len == 0) raise_err(ERR_ILLEGALQTY);
        out->num = a2mem[s.str.addr];
        return;
    }

    case T_VAL: {
        value s;
        char buf[256];
        double v = 0;
        expect('(');
        eval(&s);
        expect(')');
        if (!s.is_str) raise_err(ERR_TYPEMISMATCH);
        str_read(&s.str, buf);
        {
            char *p = buf;
            while (*p == ' ') p++;
            if (*p == '-' || *p == '+') {
                double m;
                int n = mbf_parse(p + 1, &m);
                v = n ? (*p == '-' ? -m : m) : 0;
            } else {
                if (!mbf_parse(p, &v))
                    v = 0;
            }
        }
        out->num = v;
        return;
    }

    case T_STR: {
        char buf[MBF_STRLEN];
        double v = paren_num();
        mbf_format(v, buf);
        out->is_str = 1;
        out->str = str_make(buf, (int)strlen(buf));
        return;
    }

    case T_CHR: {
        char c;
        double v = paren_num();
        if (v < 0 || v > 255) raise_err(ERR_ILLEGALQTY);
        c = (char)(int)v;
        out->is_str = 1;
        out->str = str_make(&c, 1);
        return;
    }

    case T_LEFT: case T_RIGHT: case T_MID: {
        value s;
        char buf[256];
        int n, start;
        expect('(');
        eval(&s);
        if (!s.is_str) raise_err(ERR_TYPEMISMATCH);
        expect(',');
        {
            double d = need_num();
            if (d < 0 || d > 255) raise_err(ERR_ILLEGALQTY);
            n = (int)d;
        }
        str_read(&s.str, buf);
        out->is_str = 1;

        if (t == T_LEFT) {
            expect(')');
            if (n > s.str.len) n = s.str.len;
            out->str = str_make(buf, n);
        } else if (t == T_RIGHT) {
            expect(')');
            if (n > s.str.len) n = s.str.len;
            out->str = str_make(buf + s.str.len - n, n);
        } else {
            int len;
            /* MID$(s, start) or MID$(s, start, len). start is 1-based and
             * must be at least 1; the ROM rejects 0. */
            if (n < 1) raise_err(ERR_ILLEGALQTY);
            start = n - 1;
            if (eat(',')) {
                double d = need_num();
                if (d < 0 || d > 255) raise_err(ERR_ILLEGALQTY);
                len = (int)d;
            } else {
                len = 255;
            }
            expect(')');
            if (start >= s.str.len) {
                out->str = str_make("", 0);
            } else {
                if (start + len > s.str.len)
                    len = s.str.len - start;
                out->str = str_make(buf + start, len);
            }
        }
        return;
    }
    }

    raise_err(ERR_SYNTAX);
}

static void call_user_fn(value *out)
{
    char name[3];
    int type, i;
    value arg;
    a2addr slot;
    mbf_t saved;
    const unsigned char *saved_ip;

    if (!read_name(name, &type))
        raise_err(ERR_SYNTAX);
    for (i = 0; i < MAXFN; i++)
        if (fns[i].used && strcmp(fns[i].name, name) == 0)
            break;
    if (i == MAXFN)
        raise_err(ERR_UNDEFFUNC);

    expect('(');
    eval(&arg);
    expect(')');
    if (arg.is_str)
        raise_err(ERR_TYPEMISMATCH);

    /* Applesoft evaluates the body with the parameter temporarily bound to
     * the argument, then puts the old value back. */
    slot = a2_var(fns[i].param, VT_REAL, 1);
    if (!slot)
        raise_err(ERR_OUTOFMEM);
    memcpy(saved.b, &a2mem[slot], MBF_SIZE);
    store_num(slot, arg.num);

    saved_ip = ip;
    ip = fns[i].body;
    eval(out);
    ip = saved_ip;

    memcpy(&a2mem[slot], saved.b, MBF_SIZE);
    if (out->is_str)
        raise_err(ERR_TYPEMISMATCH);
}

/* --------------------------------------------------------- control stack */

static int jumped;

/* THEN may be followed directly by a statement with no colon between, so a
 * true IF has to tell the statement loop that the next statement is allowed
 * to butt straight up against it. */
static int if_fallthrough;

static void push_frame(int kind, int bytes)
{
    if (cstack_bytes + bytes > CSTACK_BYTES ||
        ncstack >= (int)(sizeof(cstack) / sizeof(cstack[0])))
        raise_err(ERR_OUTOFMEM);
    memset(&cstack[ncstack], 0, sizeof(cstack[0]));
    cstack[ncstack].kind = kind;
    cstack[ncstack].bytes = bytes;
    ncstack++;
    cstack_bytes += bytes;
}

static void pop_frame(void)
{
    if (ncstack <= 0)
        return;
    ncstack--;
    cstack_bytes -= cstack[ncstack].bytes;
    if (cstack[ncstack].kind == FRAME_ONERR && leaked > 0)
        leaked--;
}

/* Frame sizes follow the ROM's page-1 stack, so the Machine pane's
 * "52/240" means the same thing it would on an Apple. */
#define SZ_FOR   18
#define SZ_GOSUB  6
#define SZ_ONERR  4

static int cur_offset(const unsigned char *base)
{
    return (int)(ip - base);
}

/* ---------------------------------------------------------------- output */

static void print_number(double v)
{
    char buf[MBF_STRLEN];
    mbf_format(v, buf);
    scr_puts(buf);
}

static void print_value(const value *v)
{
    if (v->is_str) {
        int i;
        for (i = 0; i < v->str.len; i++)
            scr_putc((char)a2mem[v->str.addr + i]);
    } else {
        print_number(v->num);
    }
}

/* ------------------------------------------------------------------ DATA */

static int data_active;

static void data_restore(void)
{
    data_line = a2_prog_first();
    data_offset = 0;
    data_active = 0;
}

/* Copy the next DATA item into buf. Returns 0 when the program runs out. */
static int next_data(char *buf, int *quoted)
{
    for (;;) {
        const unsigned char *t;
        int n = 0;

        if (!data_line)
            return 0;
        t = a2_prog_tokens(data_line);

        if (!data_active) {
            while (t[data_offset] && t[data_offset] != T_DATA)
                data_offset++;
            if (!t[data_offset]) {
                data_line = a2_prog_next(data_line);
                data_offset = 0;
                continue;
            }
            data_offset++;
            data_active = 1;
        }

        while (t[data_offset] == ' ')
            data_offset++;
        if (t[data_offset] == ',') {
            data_offset++;
            while (t[data_offset] == ' ')
                data_offset++;
        }

        if (!t[data_offset] || t[data_offset] == ':') {
            data_active = 0;
            if (!t[data_offset]) {
                data_line = a2_prog_next(data_line);
                data_offset = 0;
            } else {
                data_offset++;
            }
            continue;
        }

        *quoted = 0;
        if (t[data_offset] == '"') {
            *quoted = 1;
            data_offset++;
            while (t[data_offset] && t[data_offset] != '"')
                buf[n++] = (char)t[data_offset++];
            if (t[data_offset] == '"')
                data_offset++;
        } else {
            while (t[data_offset] && t[data_offset] != ',' && t[data_offset] != ':')
                buf[n++] = (char)t[data_offset++];
            /* Unquoted items keep no trailing blanks. */
            while (n > 0 && buf[n - 1] == ' ')
                n--;
        }
        buf[n] = '\0';
        return 1;
    }
}

/* ------------------------------------------------------------ statements */

static void exec_statement(void);

static void exec_line(void)
{
    for (;;) {
        while (*ip == ':')
            ip++;
        if (!*ip)
            return;
        if_fallthrough = 0;
        exec_statement();
        if (jumped || quitting)
            return;
        if (*ip && *ip != ':' && !if_fallthrough)
            raise_err(ERR_SYNTAX);
    }
}

static int read_lineno(void)
{
    int n = 0;
    if (!isdigit((unsigned char)*ip))
        raise_err(ERR_SYNTAX);
    while (isdigit((unsigned char)*ip))
        n = n * 10 + (*ip++ - '0');
    return n;
}

static void goto_line(int n)
{
    a2addr l = a2_prog_find(n);
    if (!l)
        raise_err(ERR_UNDEFSTMT);
    cur_line = l;
    ip = a2_prog_tokens(l);
    jumped = 1;
    running = 1;
}

static void do_print(void)
{
    int suppress_nl = 0;

    for (;;) {
        if (!*ip || *ip == ':')
            break;
        if (eat(';')) { suppress_nl = 1; continue; }
        if (eat(',')) { scr_comma(); suppress_nl = 1; continue; }
        if (eat(T_TABPAREN)) {
            double v = need_num();
            expect(')');
            scr_tab((int)v - 1);
            suppress_nl = 1;
            continue;
        }
        if (eat(T_SPCPAREN)) {
            double v = need_num();
            expect(')');
            scr_spc((int)v);
            suppress_nl = 1;
            continue;
        }
        {
            value v;
            eval(&v);
            print_value(&v);
            suppress_nl = 0;
        }
    }
    if (!suppress_nl)
        scr_newline();
}

static void do_input(void)
{
    char buf[256];
    char *p;

    /* An optional prompt string comes first, followed by a semicolon. */
    if (*ip == '"') {
        value v;
        primary(&v);
        print_value(&v);
        if (!eat(';'))
            (void)eat(',');
    } else {
        scr_putc('?');
    }

    if (!host_getline(buf, (int)sizeof(buf))) {
        quitting = 1;
        running = 0;
        return;
    }
    p = buf;

    do {
        int type;
        a2addr slot = lvalue(&type);
        char item[256];
        int n = 0;
        while (*p == ' ') p++;
        while (*p && *p != ',')
            item[n++] = *p++;
        while (n > 0 && item[n - 1] == ' ')
            n--;
        item[n] = '\0';
        if (*p == ',')
            p++;
        {
            value v;
            if (type == VT_STR) {
                v.is_str = 1;
                v.str = str_make(item, n);
            } else {
                double d = 0;
                v.is_str = 0;
                if (!mbf_parse(item, &d)) {
                    if (item[0] == '-')
                        mbf_parse(item + 1, &d), d = -d;
                }
                v.num = d;
            }
            store_value(slot, type, &v);
        }
    } while (eat(','));
}

static void do_read(void)
{
    do {
        int type;
        a2addr slot = lvalue(&type);
        char item[256];
        int quoted;
        value v;

        if (!next_data(item, &quoted))
            raise_err(ERR_OUTOFDATA);

        if (type == VT_STR) {
            v.is_str = 1;
            /* READ copies the item into string space; the reference build
             * charges 5 bytes for READing HELLO, and so do we. */
            v.str = str_make(item, (int)strlen(item));
        } else {
            double d = 0;
            v.is_str = 0;
            if (item[0] == '-')
                { mbf_parse(item + 1, &d); d = -d; }
            else
                mbf_parse(item, &d);
            v.num = d;
        }
        store_value(slot, type, &v);
    } while (eat(','));
}

static void do_for(void)
{
    int type;
    a2addr slot;
    double start, limit, step = 1.0;

    slot = lvalue(&type);
    if (type == VT_STR)
        raise_err(ERR_TYPEMISMATCH);
    expect(T_EQ);
    start = need_num();
    store_num(slot, start);
    expect(T_TO);
    limit = need_num();
    if (eat(T_STEP))
        step = need_num();

    push_frame(FRAME_FOR, SZ_FOR);
    {
        frame *f = &cstack[ncstack - 1];
        f->var = slot;
        f->limit = limit;
        f->step = step;
        f->line = cur_line;
        f->offset = cur_offset(cur_line ? a2_prog_tokens(cur_line) : imm);
    }
}

static void do_next(void)
{
    a2addr want = 0;
    int i;

    if (isalpha((unsigned char)*ip)) {
        int type;
        want = lvalue(&type);
    }

    /* NEXT with no variable matches the innermost loop; with one, it also
     * discards any loops nested inside it. */
    for (i = ncstack - 1; i >= 0; i--)
        if (cstack[i].kind == FRAME_FOR && (!want || cstack[i].var == want))
            break;
    if (i < 0)
        raise_err(ERR_NEXTWITHOUTFOR);
    while (ncstack > i + 1)
        pop_frame();

    {
        frame *f = &cstack[i];
        double v = load_num(f->var) + f->step;
        store_num(f->var, v);
        if ((f->step >= 0) ? (v <= f->limit) : (v >= f->limit)) {
            cur_line = f->line;
            ip = (cur_line ? a2_prog_tokens(cur_line) : imm) + f->offset;
            jumped = 1;
        } else {
            pop_frame();
        }
    }
}

static void do_list(void)
{
    int from = 0, to = 65535;
    a2addr p;
    char buf[512];

    if (isdigit((unsigned char)*ip)) {
        from = read_lineno();
        to = from;
    }
    if (eat(T_MINUS) || eat(',')) {
        to = isdigit((unsigned char)*ip) ? read_lineno() : 65535;
        if (!from)
            from = 0;
    }

    for (p = a2_prog_first(); p; p = a2_prog_next(p)) {
        int n = a2_prog_lineno(p);
        if (n < from || n > to)
            continue;
        sprintf(buf, "%d", n);
        scr_puts(buf);
        scr_putc(' ');
        tok_detokenize(a2_prog_tokens(p), buf, (int)sizeof(buf));
        scr_puts(buf);
        scr_newline();
    }
}

static void do_def(void)
{
    char name[3], param[3];
    int type, i, n = 0;

    expect(T_FN);
    if (!read_name(name, &type) || type != VT_REAL)
        raise_err(ERR_SYNTAX);
    expect('(');
    if (!read_name(param, &type) || type != VT_REAL)
        raise_err(ERR_SYNTAX);
    expect(')');
    expect(T_EQ);

    for (i = 0; i < MAXFN; i++)
        if (fns[i].used && strcmp(fns[i].name, name) == 0)
            break;
    if (i == MAXFN)
        for (i = 0; i < MAXFN; i++)
            if (!fns[i].used)
                break;
    if (i == MAXFN)
        raise_err(ERR_OUTOFMEM);

    strcpy(fns[i].name, name);
    strcpy(fns[i].param, param);
    /* The body is copied, so DEF FN typed at the prompt keeps working after
     * the input buffer is reused. */
    while (*ip && *ip != ':' && n < (int)sizeof(fns[i].body) - 1)
        fns[i].body[n++] = *ip++;
    fns[i].body[n] = 0;
    fns[i].used = 1;
}

static void do_poke(void)
{
    double a = need_num();
    long addr;
    expect(',');
    {
        double v = need_num();
        if (v < -255 || v > 255)
            raise_err(ERR_ILLEGALQTY);
        addr = (long)a;
        a2_poke((a2addr)(addr & 0xFFFF), (unsigned char)((long)v & 0xFF));
    }
}

static void do_call(void)
{
    long addr = (long)need_num();

    /* CALL -3288 is the ROM entry that pops the frame ONERR leaked, which is
     * the documented workaround ONERRFIX.BAS uses. Every other address is
     * accepted and ignored: there is no 6502 here to run. */
    if ((addr & 0xFFFF) == 0xF328) {
        int i;
        for (i = ncstack - 1; i >= 0; i--) {
            if (cstack[i].kind == FRAME_ONERR) {
                int j;
                cstack_bytes -= cstack[i].bytes;
                for (j = i; j < ncstack - 1; j++)
                    cstack[j] = cstack[j + 1];
                ncstack--;
                if (leaked > 0)
                    leaked--;
                break;
            }
        }
    }
}

/* Graphics coordinates, validated the way the ROM does. */
static int coord(int max)
{
    double v = need_num();
    if (v < 0 || v > max)
        raise_err(ERR_ILLEGALQTY);
    return (int)v;
}

static void exec_statement(void)
{
    unsigned char t = *ip;

    /* An assignment may appear with or without LET. */
    if (isalpha((unsigned char)t) || t == T_LET) {
        int type;
        a2addr slot;
        value v;
        (void)eat(T_LET);
        slot = lvalue(&type);
        expect(T_EQ);
        eval(&v);
        store_value(slot, type, &v);
        return;
    }

    ip++;
    switch (t) {
    case T_PRINT:   do_print(); return;
    case T_INPUT:   do_input(); return;
    case T_READ:    do_read(); return;
    case T_FOR:     do_for(); return;
    case T_NEXT:    do_next(); return;
    case T_LIST:    do_list(); return;
    case T_DEF:     do_def(); return;
    case T_POKE:    do_poke(); return;
    case T_CALL:    do_call(); return;
    case T_REM:     skip_to_eos(); return;
    case T_DATA:    skip_to_eos(); return;

    case T_END:     running = 0; jumped = 1; return;
    case T_STOP: {
        char buf[32];
        scr_newline();
        sprintf(buf, "BREAK IN %d", cur_line ? a2_prog_lineno(cur_line) : 0);
        scr_puts(buf);
        scr_newline();
        stopped_line = cur_line ? a2_prog_lineno(cur_line) : 0;
        stopped_offset = cur_offset(cur_line ? a2_prog_tokens(cur_line) : imm);
        running = 0;
        jumped = 1;
        return;
    }

    case T_GOTO:    goto_line(read_lineno()); return;

    case T_GOSUB: {
        int target = read_lineno();
        push_frame(FRAME_GOSUB, SZ_GOSUB);
        cstack[ncstack - 1].line = cur_line;
        cstack[ncstack - 1].offset =
            cur_offset(cur_line ? a2_prog_tokens(cur_line) : imm);
        goto_line(target);
        return;
    }

    case T_RETURN: {
        int i;
        for (i = ncstack - 1; i >= 0; i--)
            if (cstack[i].kind == FRAME_GOSUB)
                break;
        if (i < 0)
            raise_err(ERR_RETWITHOUTGOSUB);
        while (ncstack > i + 1)
            pop_frame();
        cur_line = cstack[i].line;
        ip = (cur_line ? a2_prog_tokens(cur_line) : imm) + cstack[i].offset;
        pop_frame();
        jumped = 1;
        if (cur_line)
            running = 1;
        return;
    }

    case T_POP: {
        int i;
        for (i = ncstack - 1; i >= 0; i--)
            if (cstack[i].kind == FRAME_GOSUB)
                break;
        if (i < 0)
            raise_err(ERR_RETWITHOUTGOSUB);
        while (ncstack > i)
            pop_frame();
        return;
    }

    case T_IF: {
        value c;
        eval(&c);
        if (c.is_str)
            raise_err(ERR_TYPEMISMATCH);
        /* THEN is optional before GOTO, and a bare line number after THEN
         * means GOTO that line. */
        if (!eat(T_THEN))
            (void)0;
        if (c.num == 0.0) {
            /* A false IF abandons the whole rest of the line, not just the
             * next statement. */
            while (*ip)
                ip++;
            return;
        }
        if (isdigit((unsigned char)*ip)) {
            goto_line(read_lineno());
            return;
        }
        if (eat(T_GOTO)) {
            goto_line(read_lineno());
            return;
        }
        if_fallthrough = 1;
        return;                      /* run the rest of the line */
    }

    case T_ON: {
        double sel = need_num();
        int n = (int)sel;
        int gosub = 0;
        int i = 1, target = 0;
        if (eat(T_GOSUB)) gosub = 1;
        else expect(T_GOTO);
        for (;;) {
            int l = read_lineno();
            if (i == n)
                target = l;
            i++;
            if (!eat(','))
                break;
        }
        if (n < 1 || !target)
            return;                  /* out of range simply falls through */
        if (gosub) {
            push_frame(FRAME_GOSUB, SZ_GOSUB);
            cstack[ncstack - 1].line = cur_line;
            cstack[ncstack - 1].offset =
                cur_offset(cur_line ? a2_prog_tokens(cur_line) : imm);
        }
        goto_line(target);
        return;
    }

    case T_ONERR:
        expect(T_GOTO);
        onerr_line = read_lineno();
        a2_poke(ZP_ONERRFLAG, 0xA0);      /* the ROM's "handler armed" value */
        return;

    case T_RESUME:
        if (!last_error_line)
            raise_err(ERR_SYNTAX);
        goto_line(last_error_line);
        return;

    case T_RUN: {
        a2addr start;
        if (isdigit((unsigned char)*ip)) {
            start = a2_prog_find(read_lineno());
            if (!start)
                raise_err(ERR_UNDEFSTMT);
        } else {
            start = a2_prog_first();
        }
        a2_clear_vars();
        ncstack = cstack_bytes = leaked = 0;
        onerr_line = 0;
        a2_poke(ZP_ONERRFLAG, 0);
        data_restore();
        gfx_reset();
        if (!start) { running = 0; return; }
        cur_line = start;
        ip = a2_prog_tokens(start);
        running = 1;
        jumped = 1;
        return;
    }

    case T_CONT:
        if (!stopped_line)
            raise_err(ERR_SYNTAX);
        cur_line = a2_prog_find(stopped_line);
        if (!cur_line)
            raise_err(ERR_UNDEFSTMT);
        ip = a2_prog_tokens(cur_line) + stopped_offset;
        running = 1;
        jumped = 1;
        return;

    case T_NEW:
        a2_new();
        ncstack = cstack_bytes = leaked = 0;
        memset(fns, 0, sizeof(fns));
        data_restore();
        return;

    case T_CLEAR:
        a2_clear_vars();
        ncstack = cstack_bytes = leaked = 0;
        data_restore();
        return;

    case T_RESTORE: data_restore(); return;

    case T_DIM: {
        do {
            char name[3];
            int type, idx[4], nd = 0, err;
            if (!read_name(name, &type))
                raise_err(ERR_SYNTAX);
            expect('(');
            do {
                double v = need_num();
                if (v < 0 || v > 32767)
                    raise_err(ERR_BADSUBSCRIPT);
                if (nd >= 4)
                    raise_err(ERR_BADSUBSCRIPT);
                idx[nd++] = (int)v;
            } while (eat(','));
            expect(')');
            if (!a2_array(name, type, idx, nd, 1, &err))
                raise_err(err ? err : ERR_OUTOFMEM);
        } while (eat(','));
        return;
    }

    case T_GET: {
        int type;
        a2addr slot = lvalue(&type);
        int c = host_getkey();
        value v;
        if (c <= 0) { quitting = 1; running = 0; return; }
        if (type == VT_STR) {
            char ch = (char)c;
            v.is_str = 1;
            v.str = str_make(&ch, 1);
        } else {
            v.is_str = 0;
            v.num = c;
        }
        store_value(slot, type, &v);
        return;
    }

    case T_HTAB: scr_htab((int)need_num()); return;
    case T_VTAB: scr_vtab((int)need_num()); return;
    case T_HOME: scr_home(); return;
    case T_TEXT: gfx_text(); return;

    /* Accepted and ignored: there is no speaker, no paddle, no printer. */
    case T_NORMAL: case T_INVERSE: case T_FLASH:
    case T_TRACE:  case T_NOTRACE: case T_SHLOAD:
        return;
    /* SPEED=, ROT= and SCALE= carry the "=" inside the keyword itself, so
     * there is no separate T_EQ token to consume. */
    case T_SPEED: case T_ROT: case T_SCALE:
        (void)need_num();
        return;
    case T_PRNUM: case T_INNUM:
        (void)need_num();
        return;
    case T_WAIT:
        (void)need_num();
        if (eat(',')) (void)need_num();
        if (eat(',')) (void)need_num();
        return;

    case T_HIMEM:
        a2_setword(ZP_MEMSIZ, (a2addr)((long)need_num() & 0xFFFF));
        a2_clear_vars();
        return;
    case T_LOMEM:
        a2_setword(ZP_TXTTAB, (a2addr)((long)need_num() & 0xFFFF));
        return;

    /* --- graphics: real pixels in real page memory --- */
    case T_GR:   gfx_gr(); return;
    case T_HGR:  gfx_hgr(); return;
    case T_HGR2: gfx_hgr2(); return;
    case T_COLOR:                    /* the "=" is part of the token */
        gfx_color((int)need_num());
        return;
    case T_HCOLOR:
        gfx_hcolor((int)need_num());
        return;
    case T_PLOT: {
        int x = coord(39);
        expect(',');
        gfx_plot(x, coord(47));
        return;
    }
    case T_HLIN: {
        int x1 = coord(39), x2;
        expect(',');
        x2 = coord(39);
        expect(T_AT);
        gfx_hlin(x1, x2, coord(47));
        return;
    }
    case T_VLIN: {
        int y1 = coord(47), y2;
        expect(',');
        y2 = coord(47);
        expect(T_AT);
        gfx_vlin(y1, y2, coord(39));
        return;
    }
    case T_HPLOT: {
        if (eat(T_TO)) {
            int x = coord(279);
            expect(',');
            gfx_hplot_to(x, coord(191));
        } else {
            int x = coord(279);
            expect(',');
            gfx_hplot(x, coord(191));
        }
        while (eat(T_TO)) {
            int x = coord(279);
            expect(',');
            gfx_hplot_to(x, coord(191));
        }
        return;
    }
    /* Shape tables are their own project; the arguments are still checked. */
    case T_DRAW: case T_XDRAW:
        (void)need_num();
        if (eat(T_AT)) {
            (void)need_num();
            expect(',');
            (void)need_num();
        }
        return;

    case T_LOAD: case T_SAVE: {
        char path[128];
        int n = 0;
        while (*ip == ' ')
            ip++;
        while (*ip && *ip != ':' && n < (int)sizeof(path) - 1)
            path[n++] = (char)*ip++;
        while (n > 0 && path[n - 1] == ' ')
            n--;
        path[n] = '\0';
        if (!n)
            raise_err(ERR_SYNTAX);
        if (!(t == T_LOAD ? it_load(path) : it_save(path)))
            raise_err(ERR_OUTOFMEM);
        return;
    }

    case T_DEL: {
        int from = read_lineno(), to = from;
        if (eat(T_MINUS) || eat(','))
            to = read_lineno();
        {
            a2addr p = a2_prog_first();
            while (p) {
                int n = a2_prog_lineno(p);
                a2addr nxt = a2_prog_next(p);
                if (n >= from && n <= to)
                    a2_prog_delete(n);
                p = (n >= from && n <= to) ? a2_prog_first() : nxt;
            }
        }
        return;
    }

    case T_AMP:
        skip_to_eos();
        return;
    }

    raise_err(ERR_SYNTAX);
}

/* ---------------------------------------------------------- error report */

static void report_error(int code)
{
    char buf[64];
    /* The reference always breaks the line before an error, even at column
     * zero, which is why a failed immediate command leaves the "]" on a line
     * of its own and a failure mid-program prints a blank line first. */
    scr_newline();
    if (running && cur_line)
        sprintf(buf, "?%s IN %d", err_message(code), a2_prog_lineno(cur_line));
    else
        sprintf(buf, "?%s", err_message(code));
    scr_puts(buf);
    scr_newline();
}

/* Returns 1 when execution should resume at an ONERR handler. */
static int handle_error(int code)
{
    last_error = code;
    last_error_line = cur_line ? a2_prog_lineno(cur_line) : 0;
    a2_poke(ZP_ERRNUM, (unsigned char)code);
    a2_setword(ZP_ERRLIN, (a2addr)last_error_line);

    if (running && onerr_line && a2_peek(ZP_ONERRFLAG)) {
        a2addr handler = a2_prog_find(onerr_line);
        if (!handler) {
            report_error(ERR_UNDEFSTMT);
            running = 0;
            return 0;
        }
        /* The leak: trapping an error pushes a frame the ROM never pops, so
         * a program that traps in a loop eventually dies of OUT OF MEMORY.
         * CALL -3288 is the way out, which is what ONERRFIX.BAS shows. */
        if (bug_enabled[BUG_ONERR_LEAK]) {
            if (cstack_bytes + SZ_ONERR > CSTACK_BYTES) {
                report_error(ERR_OUTOFMEM);
                running = 0;
                return 0;
            }
            memset(&cstack[ncstack], 0, sizeof(cstack[0]));
            cstack[ncstack].kind = FRAME_ONERR;
            cstack[ncstack].bytes = SZ_ONERR;
            ncstack++;
            cstack_bytes += SZ_ONERR;
            leaked++;
        }
        cur_line = handler;
        ip = a2_prog_tokens(handler);
        jumped = 0;
        return 1;
    }

    report_error(code);
    running = 0;
    return 0;
}

/* ----------------------------------------------------------------- entry */

void it_init(void)
{
    a2_init();
    gfx_reset();
    memset(fns, 0, sizeof(fns));
    ncstack = cstack_bytes = leaked = 0;
    cur_line = 0;
    running = quitting = 0;
    onerr_line = 0;
    last_error = ERR_NONE;
    last_error_line = 0;
    stopped_line = stopped_offset = 0;
    data_restore();
}

void it_line(const char *src)
{
    int len;

    while (*src == ' ')
        src++;
    if (!*src)
        return;

    /* A leading line number stores the line instead of running it. */
    if (isdigit((unsigned char)*src)) {
        char *end;
        long n = strtol(src, &end, 10);
        unsigned char toks[512];
        while (*end == ' ')
            end++;
        if (n < 0 || n > 63999) {
            scr_newline();
            scr_puts("?SYNTAX ERROR");
            scr_newline();
            return;
        }
        len = tok_tokenize(end, toks, (int)sizeof(toks),
                           bug_enabled[BUG_GREEDY_TOKENIZER]);
        if (len < 0) {
            scr_newline();
            scr_puts("?STRING TOO LONG");
            scr_newline();
            return;
        }
        if (!a2_prog_insert((int)n, toks, len)) {
            scr_newline();
            scr_puts("?OUT OF MEMORY");
            scr_newline();
        }
        data_restore();
        return;
    }

    len = tok_tokenize(src, imm, (int)sizeof(imm),
                       bug_enabled[BUG_GREEDY_TOKENIZER]);
    if (len < 0) {
        scr_newline();
        scr_puts("?STRING TOO LONG");
        scr_newline();
        return;
    }

    cur_line = 0;
    ip = imm;
    jumped = 0;
    running = 0;

    for (;;) {
        int code = setjmp(err_jmp);
        if (code == 0) {
            exec_line();
            while (running && !quitting) {
                if (jumped) {
                    jumped = 0;
                } else {
                    cur_line = a2_prog_next(cur_line);
                    if (!cur_line) { running = 0; break; }
                    ip = a2_prog_tokens(cur_line);
                }
                if (host_break()) {
                    char buf[32];
                    scr_newline();
                    sprintf(buf, "BREAK IN %d", a2_prog_lineno(cur_line));
                    scr_puts(buf);
                    scr_newline();
                    stopped_line = a2_prog_lineno(cur_line);
                    stopped_offset = 0;
                    running = 0;
                    break;
                }
                exec_line();
            }
            break;
        }
        if (!handle_error(code - 1))
            break;
    }
    running = 0;
}

int it_quitting(void) { return quitting; }

/* ------------------------------------------------------------- inspection */

int it_cstack_used(void)  { return cstack_bytes; }
int it_cstack_count(void) { return ncstack; }
int it_leaked_frames(void) { return leaked; }
int it_last_error(void)   { return last_error; }
int it_last_error_line(void) { return last_error_line; }
int it_current_line(void) { return cur_line ? a2_prog_lineno(cur_line) : 0; }

int it_frame_info(int i, int *kind, char *label, long *value)
{
    if (i < 0 || i >= ncstack)
        return 0;
    *kind = cstack[i].kind;
    switch (cstack[i].kind) {
    case FRAME_FOR: {
        /* Recover the loop variable's name from its slot. */
        int j, n = a2_var_count();
        label[0] = '?'; label[1] = '\0';
        for (j = 0; j < n; j++) {
            char nm[3]; int ty; a2addr slot;
            if (a2_var_info(j, nm, &ty, &slot) && slot == cstack[i].var) {
                strcpy(label, nm);
                break;
            }
        }
        *value = (long)load_num(cstack[i].var);
        return 1;
    }
    case FRAME_GOSUB:
        strcpy(label, "");
        *value = cstack[i].line ? a2_prog_lineno(cstack[i].line) : 0;
        return 1;
    default:
        strcpy(label, "leaked");
        *value = 0;
        return 1;
    }
}

/* ------------------------------------------------------------ load / save */

int it_load(const char *path)
{
    char line[512];
    unsigned char toks[512];
    FILE *f = fopen(path, "r");
    if (!f)
        return 0;

    a2_new();
    memset(fns, 0, sizeof(fns));
    while (fgets(line, (int)sizeof(line), f)) {
        char *p = line, *end;
        long n;
        int len = (int)strlen(line);
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        while (*p == ' ')
            p++;
        if (!*p)
            continue;
        if (!isdigit((unsigned char)*p))
            continue;             /* not a numbered line; skip it */
        n = strtol(p, &end, 10);
        while (*end == ' ')
            end++;
        len = tok_tokenize(end, toks, (int)sizeof(toks),
                           bug_enabled[BUG_GREEDY_TOKENIZER]);
        if (len < 0)
            continue;
        if (!a2_prog_insert((int)n, toks, len)) {
            fclose(f);
            return 0;
        }
    }
    fclose(f);
    data_restore();
    return 1;
}

int it_save(const char *path)
{
    char buf[512];
    a2addr p;
    FILE *f = fopen(path, "w");
    if (!f)
        return 0;
    for (p = a2_prog_first(); p; p = a2_prog_next(p)) {
        tok_detokenize(a2_prog_tokens(p), buf, (int)sizeof(buf));
        fprintf(f, "%d %s\n", a2_prog_lineno(p), buf);
    }
    fclose(f);
    return 1;
}
