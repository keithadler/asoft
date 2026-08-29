/* mbf.h - Microsoft Binary Format floats, as Applesoft stores them.
 *
 * Applesoft keeps every numeric value in five bytes:
 *
 *     exp  m1  m2  m3  m4
 *
 * exp is excess-128; exp == 0 means the value is zero and the mantissa bytes
 * are ignored. The mantissa is normalised so its top bit is always 1, which
 * means that bit carries no information and the sign is stored in its place
 * (bit 7 of m1). Restoring the implied bit gives a 32-bit mantissa in
 * [2^31, 2^32), and the value is
 *
 *     (-1)^sign * (mantissa / 2^32) * 2^(exp - 128)
 *
 * so $81 $00 $00 $00 $00 is 1.0. That is roughly 9.6 decimal digits of
 * precision, which is why Applesoft prints nine and no more.
 *
 * Arithmetic runs in the host's double and is rounded back to MBF width on
 * every store. Doing the sums in double and narrowing afterwards is not the
 * same as the ROM's own routines bit for bit, but it lands on the same
 * nine printed digits, which is the part anything can observe.
 */
#ifndef ASOFT_MBF_H
#define ASOFT_MBF_H

#define MBF_SIZE 5

/* Largest and smallest magnitudes an exponent byte can hold. */
#define MBF_MAX 1.70141183e38
#define MBF_MIN 1.46936794e-39

typedef struct {
    unsigned char b[MBF_SIZE];
} mbf_t;

/* Pack/unpack. mbf_pack rounds to nearest, ties away from zero, and reports
 * overflow so the caller can raise ?OVERFLOW. */
int  mbf_pack(double v, mbf_t *out);      /* 1 ok, 0 overflow (out = +/-max) */
double mbf_unpack(const mbf_t *m);

void mbf_set_zero(mbf_t *m);
int  mbf_is_zero(const mbf_t *m);

/* Round v to the precision MBF can actually hold. Every arithmetic result
 * passes through this, which is what keeps .1 + .2 printing as .3. */
double mbf_round(double v);

/* Applesoft's PRINT representation of v. buf needs MBF_STRLEN bytes.
 * Nine significant digits; fixed point when 0.01 <= |v| < 1E9, otherwise
 * scientific with a two-digit exponent. No leading zero before the point and
 * no leading space for positive values -- "-12345.678", ".3", "1E+09". */
#define MBF_STRLEN 24
void mbf_format(double v, char *buf);

/* Parse a numeric literal at s. Returns the number of characters consumed
 * (0 if s does not start one) and stores the value through out.
 *
 * Unlike the Watcom console build shipped in reference/, this accepts a signed
 * exponent: that build stops after the "E" of "1E-5", leaving the "-5" to be
 * parsed as subtraction, so it prints -4. The ROM accepts it, so we do too. */
int mbf_parse(const char *s, double *out);

#endif
