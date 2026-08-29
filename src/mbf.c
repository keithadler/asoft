#include "mbf.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void mbf_set_zero(mbf_t *m)
{
    memset(m->b, 0, MBF_SIZE);
}

int mbf_is_zero(const mbf_t *m)
{
    return m->b[0] == 0;
}

int mbf_pack(double v, mbf_t *out)
{
    int exp, negative;
    double frac;
    unsigned long mant;

    if (v == 0.0) {
        mbf_set_zero(out);
        return 1;
    }

    negative = (v < 0.0);
    if (negative)
        v = -v;

    /* frexp gives v = frac * 2^exp with frac in [0.5, 1), which is exactly
     * the normalisation MBF wants once frac is scaled by 2^32. */
    frac = frexp(v, &exp);

    /* 4294967296.0 is 2^32; written out so no integer constant has to be
     * wider than the 16-bit int of the DOS build. */
    mant = (unsigned long)(frac * 4294967296.0 + 0.5);

    /* Rounding up can carry out of the top of the mantissa. */
    if (mant < 2147483648UL) {          /* 2^31 - only if frac rounded down */
        mant <<= 1;
        exp--;
    } else if (frac * 4294967296.0 + 0.5 >= 4294967296.0) {
        mant = 2147483648UL;
        exp++;
    }

    exp += 128;
    if (exp > 255) {
        /* Saturate so the caller still has a usable value to print if it
         * chooses to ignore the overflow. */
        out->b[0] = 255;
        out->b[1] = (unsigned char)(negative ? 0xFF : 0x7F);
        out->b[2] = out->b[3] = out->b[4] = 0xFF;
        return 0;
    }
    if (exp < 1) {
        /* Underflow is silent in Applesoft: the value simply becomes zero. */
        mbf_set_zero(out);
        return 1;
    }

    out->b[0] = (unsigned char)exp;
    out->b[1] = (unsigned char)((mant >> 24) & 0xFF);
    out->b[2] = (unsigned char)((mant >> 16) & 0xFF);
    out->b[3] = (unsigned char)((mant >> 8) & 0xFF);
    out->b[4] = (unsigned char)(mant & 0xFF);

    /* The implied top bit is always 1, so bit 7 of m1 is free for the sign. */
    if (negative)
        out->b[1] |= 0x80;
    else
        out->b[1] &= 0x7F;

    return 1;
}

double mbf_unpack(const mbf_t *m)
{
    unsigned long mant;
    double v;
    int exp;

    if (m->b[0] == 0)
        return 0.0;

    exp = (int)m->b[0] - 128;
    mant = ((unsigned long)(m->b[1] | 0x80) << 24) |
           ((unsigned long)m->b[2] << 16) |
           ((unsigned long)m->b[3] << 8) |
           (unsigned long)m->b[4];

    v = ldexp((double)mant / 4294967296.0, exp);
    return (m->b[1] & 0x80) ? -v : v;
}

double mbf_round(double v)
{
    mbf_t m;
    mbf_pack(v, &m);
    return mbf_unpack(&m);
}

void mbf_format(double v, char *buf)
{
    char digits[32];
    char *p = buf;
    int dexp, i, ndig;
    int negative;

    if (v == 0.0) {
        strcpy(buf, "0");
        return;
    }

    negative = (v < 0.0);
    if (negative)
        v = -v;

    /* Round to the nine significant digits Applesoft prints, and read the
     * decimal exponent back off the result so the carry in 9999999999 ->
     * 1E+10 is already accounted for. */
    sprintf(digits, "%.8E", v);
    {
        char *e = strchr(digits, 'E');
        dexp = atoi(e + 1);
        *e = '\0';
    }

    /* digits now looks like "1.07374182"; strip the point to get the mantissa
     * digits on their own. */
    {
        char m[16];
        int n = 0;
        for (i = 0; digits[i]; i++)
            if (digits[i] != '.')
                m[n++] = digits[i];
        m[n] = '\0';
        /* Applesoft never shows trailing zeros. */
        while (n > 1 && m[n - 1] == '0')
            m[--n] = '\0';
        ndig = n;
        strcpy(digits, m);
    }

    if (negative)
        *p++ = '-';

    if (dexp >= -2 && dexp <= 8) {
        /* Fixed point. */
        if (dexp < 0) {
            *p++ = '.';
            for (i = 0; i < -dexp - 1; i++)
                *p++ = '0';
            for (i = 0; i < ndig; i++)
                *p++ = digits[i];
        } else {
            for (i = 0; i <= dexp; i++)
                *p++ = (i < ndig) ? digits[i] : '0';
            if (ndig > dexp + 1) {
                *p++ = '.';
                for (i = dexp + 1; i < ndig; i++)
                    *p++ = digits[i];
            }
        }
    } else {
        /* Scientific, always a signed two-digit exponent. */
        *p++ = digits[0];
        if (ndig > 1) {
            *p++ = '.';
            for (i = 1; i < ndig; i++)
                *p++ = digits[i];
        }
        *p++ = 'E';
        *p++ = (dexp < 0) ? '-' : '+';
        {
            int a = dexp < 0 ? -dexp : dexp;
            *p++ = (char)('0' + (a / 10) % 10);
            *p++ = (char)('0' + a % 10);
        }
    }
    *p = '\0';
}

int mbf_literal_len(const char *s)
{
    const char *start = s;
    int seen = 0;

    while (*s >= '0' && *s <= '9') { s++; seen = 1; }
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9') { s++; seen = 1; }
    }
    if (!seen)
        return 0;

    if (*s == 'E' || *s == 'e') {
        const char *e = s + 1;
        if (*e == '+' || *e == '-')
            e++;
        /* Only commit to the exponent if a digit actually follows, so "1E"
         * on its own stays 1 rather than swallowing the E. */
        if (*e >= '0' && *e <= '9') {
            while (*e >= '0' && *e <= '9')
                e++;
            s = e;
        }
    }
    return (int)(s - start);
}

int mbf_parse(const char *s, double *out)
{
    const char *start = s;
    int n = mbf_literal_len(s);

    if (!n)
        return 0;
    s += n;

    {
        char tmp[40];
        int n = (int)(s - start);
        if (n > (int)sizeof(tmp) - 1)
            n = (int)sizeof(tmp) - 1;
        memcpy(tmp, start, (size_t)n);
        tmp[n] = '\0';
        *out = mbf_round(atof(tmp));
    }
    return (int)(s - start);
}
