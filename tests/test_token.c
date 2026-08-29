/* Round-trips source through tokenize + detokenize and compares against the
 * LIST output captured from the reference DOS build. The expectations are the
 * part of each listed line that follows the line number and its single space,
 * so "10  REM  HI" contributes " REM  HI". */
#include "../src/token.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;

static void chk(const char *src, const char *want)
{
    unsigned char toks[512];
    char out[512];
    int n = tok_tokenize(src, toks, (int)sizeof(toks), 1);
    if (n < 0) {
        printf("  FAIL  tokenize overflow: %s\n", src);
        failures++;
        return;
    }
    tok_detokenize(toks, out, (int)sizeof(out));
    if (strcmp(out, want) != 0) {
        printf("  FAIL  %s\n        want [%s]\n        got  [%s]\n", src, want, out);
        failures++;
    }
}

int main(void)
{
    /* Every pair below is straight out of tools/capture/captured/tests.txt. */
    chk("REM APPLESOFT COMPATIBILITY CHECKS", " REM  APPLESOFT COMPATIBILITY CHECKS");
    chk("PRINT \"FLOATS:\"", " PRINT \"FLOATS:\"");
    chk("PRINT .1 + .2;\" \";1/3;\" \";2^10;\" \";SQR(2)",
        " PRINT .1 + .2;\" \";1 / 3;\" \";2 ^ 10;\" \"; SQR (2)");
    chk("PRINT 1E9;\" \";.001;\" \";.01;\" \";999999999",
        " PRINT 1E9;\" \";.001;\" \";.01;\" \";999999999");
    chk("PRINT -12345.678;\" \";3.14159265", " PRINT  - 12345.678;\" \";3.14159265");
    chk("PRINT", " PRINT ");
    chk("A$ = \"APPLESOFT\": PRINT LEFT$(A$,5);\"-\";MID$(A$,6,4);\"-\";RIGHT$(A$,4)",
        "A$ = \"APPLESOFT\": PRINT  LEFT$ (A$,5);\"-\"; MID$ (A$,6,4);\"-\"; RIGHT$ (A$,4)");
    chk("PRINT LEN(A$);\" \";ASC(\"A\");\" \";CHR$(66);\" \";VAL(\"12.5\")+1",
        " PRINT  LEN (A$);\" \"; ASC (\"A\");\" \"; CHR$ (66);\" \"; VAL (\"12.5\") + 1");
    chk("X = 0: FOR I = 1 TO 100: X = X + I: NEXT: PRINT \"SUM=\";X",
        "X = 0: FOR I = 1 TO 100:X = X + I: NEXT : PRINT \"SUM=\";X");
    chk("DIM B(10): FOR I = 0 TO 10: B(I) = I * I: NEXT",
        " DIM B(10): FOR I = 0 TO 10:B(I) = I * I: NEXT ");
    chk("PRINT \"B(3)=\";B(3);\" B(10)=\";B(10)",
        " PRINT \"B(3)=\";B(3);\" B(10)=\";B(10)");
    chk("GOSUB 900", " GOSUB 900");
    chk("DATA 11,22,HELLO", " DATA  11,22,HELLO");
    chk("READ P,Q,R$: PRINT P + Q;\" \";R$", " READ P,Q,R$: PRINT P + Q;\" \";R$");
    chk("HTAB 10: P = POS(0): PRINT \"POS AFTER HTAB 10 = \";P;\" (ROM SAYS 11)\"",
        " HTAB 10:P =  POS (0): PRINT \"POS AFTER HTAB 10 = \";P;\" (ROM SAYS 11)\"");
    chk("PRINT \"FRE(0)=\";FRE(0)", " PRINT \"FRE(0)=\"; FRE (0)");
    chk("ONERR GOTO 300", " ONERR  GOTO 300");
    chk("N = 0", "N = 0");
    chk("X = 1/0", "X = 1 / 0");
    chk("N = N + 1: IF N < 100 THEN 250", "N = N + 1: IF N < 100 THEN 250");
    chk("END", " END ");
    chk("PRINT \"GOSUB OK\": RETURN", " PRINT \"GOSUB OK\": RETURN ");

    /* And from captured/probe-era runs. */
    chk("X = 1: PRINT X", "X = 1: PRINT X");
    chk("A = B ^ 2", "A = B ^ 2");
    chk("IF X <> 0 THEN PRINT \"HI\"", " IF X <  > 0 THEN  PRINT \"HI\"");
    chk("FORI=1TO10:NEXTI", " FOR I = 1 TO 10: NEXT I");
    chk("PRINT\"A\";TAB(20);\"B\"", " PRINT \"A\"; TAB( 20);\"B\"");

    /* The greedy tokenizer eats TO out of TOTAL; switching it off does not. */
    {
        unsigned char toks[64];
        char out[128];
        tok_tokenize("TOTAL = 5", toks, (int)sizeof(toks), 1);
        tok_detokenize(toks, out, (int)sizeof(out));
        if (strcmp(out, " TO TAL = 5") != 0) {
            printf("  FAIL  greedy TOTAL want [ TO TAL = 5] got [%s]\n", out);
            failures++;
        }
        tok_tokenize("TOTAL = 5", toks, (int)sizeof(toks), 0);
        tok_detokenize(toks, out, (int)sizeof(out));
        if (strcmp(out, "TOTAL = 5") != 0) {
            printf("  FAIL  non-greedy TOTAL want [TOTAL = 5] got [%s]\n", out);
            failures++;
        }
    }

    /* A number keeps its signed exponent: leave it to the keyword matcher
     * and 1E-5 becomes 1 - 5, which is what the reference prints. */
    chk("PRINT 1E-5", " PRINT 1E-5");
    chk("PRINT 2E+3;1.5E-10", " PRINT 2E+3;1.5E-10");
    chk("PRINT 1E", " PRINT 1E");          /* bare E is a variable, not an exponent */

    /* LOAD and SAVE keep their tail, so a DOS path survives tokenizing. */
    chk("LOAD web/bundle/TESTS.BAS", " LOAD  web/bundle/TESTS.BAS");
    chk("SAVE C:\\PROGS\\A.BAS", " SAVE  C:\\PROGS\\A.BAS");

    /* Non-greedy mode must still see keywords that follow a number or a
     * symbol, and only refuse the ones welded into an identifier. */
    {
        unsigned char toks[128];
        char out[256];
        tok_tokenize("FOR I = 1 TO 10", toks, (int)sizeof(toks), 0);
        tok_detokenize(toks, out, (int)sizeof(out));
        if (strcmp(out, " FOR I = 1 TO 10") != 0) {
            printf("  FAIL  non-greedy FOR want [ FOR I = 1 TO 10] got [%s]\n", out);
            failures++;
        }
    }

    /* SAVE writes a form that LOAD turns back into identical tokens, so a
     * round trip does not keep adding spaces to REM and DATA tails. */
    {
        static const char *lines[] = {
            "REM APPLESOFT COMPATIBILITY CHECKS",
            "DATA 11,22,HELLO",
            "PRINT \"HI\": REM TRAILING",
            "X = 1: FOR I = 1 TO 3: NEXT",
            0
        };
        int i;
        for (i = 0; lines[i]; i++) {
            unsigned char a[512], b[512];
            char src[512];
            int na = tok_tokenize(lines[i], a, (int)sizeof(a), 1);
            int nb;
            tok_detokenize_src(a, src, (int)sizeof(src));
            nb = tok_tokenize(src, b, (int)sizeof(b), 1);
            if (na != nb || memcmp(a, b, (size_t)na) != 0) {
                printf("  FAIL  save/load round trip: %s\n        via [%s]\n",
                       lines[i], src);
                failures++;
            }
        }
    }

    /* AT is a prefix of ATN, and the table lists AT first. Matching the first
     * keyword rather than the longest turned ATN(1) into AT followed by the
     * variable N: a syntax error, which LIST showed back as "AT N(1)". The
     * greedy bug must survive the fix, though -- TOTAL is not a keyword, so it
     * still comes apart into TO and TAL. */
    {
        unsigned char t[64];
        char back[128];

        tok_tokenize("PRINT ATN(1)", t, (int)sizeof(t), 1);
        if (!memchr(t, T_ATN, sizeof(t))) {
            printf("  FAIL  ATN did not tokenize to T_ATN\n");
            failures++;
        }
        if (memchr(t, T_AT, sizeof(t))) {
            printf("  FAIL  ATN tokenized as AT + N\n");
            failures++;
        }
        tok_detokenize(t, back, (int)sizeof(back));
        if (!strstr(back, "ATN")) {
            printf("  FAIL  ATN listed back as \"%s\"\n", back);
            failures++;
        }

        tok_tokenize("10 TOTAL = 5", t, (int)sizeof(t), 1);
        tok_detokenize(t, back, (int)sizeof(back));
        if (!strstr(back, "TO TAL")) {
            printf("  FAIL  the greedy bug stopped working: \"%s\"\n", back);
            failures++;
        }
    }

    printf("test_token: %s\n", failures ? "FAILED" : "ok");
    return failures ? 1 : 0;
}
