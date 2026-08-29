/* Checks mbf_format against output captured from the reference DOS build
  (tools/capture/captured/). Every expectation here is a string that
 * binary actually printed, not a guess at what Applesoft ought to do. */
#include "../src/mbf.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int failures = 0;

static void chk(double v, const char *want)
{
    char got[MBF_STRLEN];
    mbf_format(mbf_round(v), got);
    if (strcmp(got, want) != 0) {
        printf("  FAIL  %-22s want %-18s got %s\n", "", want, got);
        failures++;
    }
}

static void chk_parse(const char *src, const char *want)
{
    double v = 0;
    char got[MBF_STRLEN];
    int n = mbf_parse(src, &v);
    if (n == 0) {
        printf("  FAIL  parse %-16s consumed nothing\n", src);
        failures++;
        return;
    }
    mbf_format(v, got);
    if (strcmp(got, want) != 0) {
        printf("  FAIL  parse %-16s want %-14s got %s\n", src, want, got);
        failures++;
    }
}

int main(void)
{
    /* Straight from the captured transcript. */
    chk(0.0, "0");
    chk(1.0, "1");
    chk(-1.0, "-1");
    chk(mbf_round(0.1) + mbf_round(0.2), ".3");
    chk(1.0 / 3.0, ".333333333");
    chk(pow(2.0, 10.0), "1024");
    chk(sqrt(2.0), "1.41421356");
    chk(1e9, "1E+09");
    chk(999999999.0, "999999999");
    chk(1000000000.0, "1E+09");
    chk(0.01, ".01");
    chk(0.001, "1E-03");
    chk(-12345.678, "-12345.678");
    chk(3.14159265, "3.14159265");
    chk(100.0 / 7.0, "14.2857143");
    chk(pow(2.0, 30.0), "1.07374182E+09");
    chk(0.0099, "9.9E-03");
    chk(0.0100001, ".0100001");
    chk(9999999999.0, "1E+10");
    chk(123456789012.0, "1.23456789E+11");
    chk(1.23456789012, "1.23456789");
    chk(2.0 / 3.0, ".666666667");
    chk(-2.0 / 3.0, "-.666666667");
    chk(0.5, ".5");

    /* Values TESTS.BAS prints. */
    chk(5050.0, "5050");
    chk(9.0, "9");
    chk(65.0, "65");
    chk(13.5, "13.5");
    chk(100.0, "100");
    chk(35491.0, "35491");
    chk(11.0, "11");
    chk(33.0, "33");
    chk(-3.0, "-3");

    /* Literal parsing, including the signed exponent the reference build
     * mishandles (it prints -4 for 1E-5). */
    chk_parse("1", "1");
    chk_parse(".5", ".5");
    chk_parse("1E9", "1E+09");
    chk_parse("1E-5", "1E-05");
    chk_parse("1E+3", "1000");
    chk_parse("12.5", "12.5");
    chk_parse("1E", "1");          /* bare E is not an exponent */

    /* Pack/unpack should be exact for values MBF can represent. */
    {
        mbf_t m;
        mbf_pack(1.0, &m);
        if (!(m.b[0] == 0x81 && m.b[1] == 0 && m.b[2] == 0 && m.b[3] == 0 && m.b[4] == 0)) {
            printf("  FAIL  1.0 packs to %02X %02X %02X %02X %02X, want 81 00 00 00 00\n",
                   m.b[0], m.b[1], m.b[2], m.b[3], m.b[4]);
            failures++;
        }
        if (mbf_unpack(&m) != 1.0) { printf("  FAIL  unpack(1.0)\n"); failures++; }
        mbf_pack(-1.0, &m);
        if (mbf_unpack(&m) != -1.0) { printf("  FAIL  unpack(-1.0)\n"); failures++; }
        mbf_pack(0.0, &m);
        if (!mbf_is_zero(&m)) { printf("  FAIL  zero\n"); failures++; }
    }

    printf("test_mbf: %s\n", failures ? "FAILED" : "ok");
    return failures ? 1 : 0;
}
