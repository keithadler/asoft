#include "dos33.h"
#include "a2mem.h"
#include "errs.h"
#include "gfx.h"
#include "interp.h"
#include "printer.h"
#include "screen.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __DOS__
#include <direct.h>
#else
#include <dirent.h>
#endif

/* DOS 3.3's MAXFILES default: three files open at once. */
#define MAXFILES 3

typedef struct {
    char   path[128];
    FILE  *f;
    long   reclen;                   /* L option, 0 for sequential */
} dosfile;

static dosfile files[MAXFILES];
static int rd = -1, wr = -1;        /* the file READ and WRITE point at */

static int capturing;               /* a control-D came at line start */
static char cmd[256];
static int cmdlen;
static int at_line_start = 1;

static void (*error_hook)(int code);
static void (*program_hook)(const char *path, int run);

void dos_set_error_hook(void (*raise)(int)) { error_hook = raise; }
void dos_set_program_hook(void (*load)(const char *, int)) { program_hook = load; }

static void fail(int code)
{
    capturing = 0;
    rd = wr = -1;
    if (error_hook)
        error_hook(code);
}

void dos_init(void)
{
    int i;
    for (i = 0; i < MAXFILES; i++) {
        files[i].f = 0;
        files[i].path[0] = '\0';
        files[i].reclen = 0;
    }
    rd = wr = -1;
    capturing = 0;
    at_line_start = 1;
}

/* --- names ---------------------------------------------------------------
 * A DOS 3.3 name was up to thirty characters, spaces allowed, no type in
 * the name. Here the type is an extension, and the name is taken as typed
 * -- upper-cased, since that is how the machine saw it -- unless it already
 * has an extension or a path, in which case it is a host file the caller
 * knows about. A DOS build has to live within eight characters and no
 * spaces, so there the stem is squeezed to fit. */
void dos_path(const char *name, const char *ext, char *out, int max)
{
    int n = 0, has_ext = 0, has_dir = 0;
    const char *p;

    while (*name == ' ')
        name++;
    for (p = name; *p; p++) {
        if (*p == '.') has_ext = 1;
        if (*p == '/' || *p == '\\' || *p == ':') { has_dir = 1; has_ext = 0; }
    }
    for (p = name; *p && n < max - 6; p++) {
        char c = *p;
        if (!has_dir) {
            c = (char)toupper((unsigned char)c);
#ifdef __DOS__
            if (c == ' ') continue;
            if (!has_ext && n >= 8) break;
#endif
        }
        out[n++] = c;
    }
    while (n > 0 && out[n - 1] == ' ')
        n--;
    out[n] = '\0';
    if (!has_ext && n > 0)
        strcat(out, ext);
}

/* --- the file table ------------------------------------------------------ */

static int find_open(const char *path)
{
    int i;
    for (i = 0; i < MAXFILES; i++)
        if (files[i].f && strcmp(files[i].path, path) == 0)
            return i;
    return -1;
}

static int exists(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return 0;
    fclose(f);
    return 1;
}

/* OPEN: find the file or make it, as DOS did. */
static int open_file(const char *path, long reclen)
{
    int i = find_open(path);
    FILE *f;

    if (i >= 0)
        return i;
    for (i = 0; i < MAXFILES && files[i].f; i++)
        ;
    if (i == MAXFILES) {
        fail(ERR_NOBUFFERS);
        return -1;
    }
    f = fopen(path, "r+");
    if (!f)
        f = fopen(path, "w+");
    if (!f) {
        fail(ERR_WRITEPROT);
        return -1;
    }
    files[i].f = f;
    files[i].reclen = reclen;
    strncpy(files[i].path, path, sizeof(files[i].path) - 1);
    files[i].path[sizeof(files[i].path) - 1] = '\0';
    return i;
}

static void close_slot(int i)
{
    if (files[i].f)
        fclose(files[i].f);
    files[i].f = 0;
    files[i].path[0] = '\0';
    if (rd == i) rd = -1;
    if (wr == i) wr = -1;
}

void dos_close_all(void)
{
    int i;
    for (i = 0; i < MAXFILES; i++)
        close_slot(i);
}

void dos_reset_modes(void)
{
    rd = wr = -1;
    capturing = 0;
    at_line_start = 1;
}

/* --- reading ------------------------------------------------------------ */

int dos_reading(void) { return rd >= 0; }

int dos_read_line(char *buf, int max)
{
    FILE *f = files[rd].f;
    int n = 0, c;

    for (;;) {
        c = fgetc(f);
        if (c == EOF) {
            if (n == 0)
                return 0;
            break;
        }
        if (c == '\r') {
            int d = fgetc(f);
            if (d != '\n' && d != EOF)
                ungetc(d, f);
            break;
        }
        if (c == '\n')
            break;
        if (n < max - 1)
            buf[n++] = (char)c;
    }
    buf[n] = '\0';
    return 1;
}

int dos_read_char(void)
{
    int c = fgetc(files[rd].f);
    if (c == EOF)
        return -1;
    if (c == '\n')
        c = '\r';                        /* the Apple's end of line */
    return c;
}

/* --- the filter --------------------------------------------------------- */

int dos_filter(char ch)
{
    if (capturing) {
        if (ch == '\n') {
            cmd[cmdlen] = '\0';
            capturing = 0;
            dos_command(cmd);
        } else if (cmdlen < (int)sizeof(cmd) - 1) {
            cmd[cmdlen++] = ch;
        }
        return 1;
    }
    if (ch == 4) {
        /* Only at the start of a line was a control-D a command; anywhere
         * else DOS let it through, and the screen showed nothing for it. */
        if (at_line_start) {
            capturing = 1;
            cmdlen = 0;
        }
        return 1;
    }
    at_line_start = (ch == '\n');
    if (wr >= 0) {
        if (ch == '\n' || (unsigned char)ch >= 32)
            fputc(ch, files[wr].f);
        return 1;
    }
    return 0;
}

/* --- commands ------------------------------------------------------------ */

/* Split "NAME,L20,R3" into the name and its options. */
typedef struct {
    char name[128];
    char name2[128];                 /* RENAME's second name */
    long L, R, B, A;
    int  hasL, hasR, hasB, hasA;
} args_t;

static long number(const char *s)
{
    if (*s == '$')
        return strtol(s + 1, 0, 16);
    return strtol(s, 0, 10);
}

/* Is this comma-separated piece an option -- a letter, then nothing or a
 * number, "L20", "R3", "A$800", "V0", MON's "C" -- rather than a name? */
static int is_option(const char *q)
{
    if (!isalpha((unsigned char)*q))
        return 0;
    q++;
    if (*q == '$')
        q++;
    while (isdigit((unsigned char)*q))
        q++;
    while (*q == ' ')
        q++;
    return *q == '\0' || *q == ',';
}

static void parse_args(const char *s, args_t *a)
{
    const char *p = s;
    int n = 0;

    memset(a, 0, sizeof(*a));
    while (*p == ' ')
        p++;
    while (*p && *p != ',' && n < 127)
        a->name[n++] = *p++;
    a->name[n] = '\0';
    while (n > 0 && a->name[n - 1] == ' ')
        a->name[--n] = '\0';

    while (*p == ',') {
        const char *q = p + 1;
        while (*q == ' ')
            q++;
        if (is_option(q)) {
            switch (toupper((unsigned char)*q)) {
            case 'L': a->L = number(q + 1); a->hasL = 1; break;
            case 'R': a->R = number(q + 1); a->hasR = 1; break;
            case 'B': a->B = number(q + 1); a->hasB = 1; break;
            case 'A': a->A = number(q + 1); a->hasA = 1; break;
            default: break;              /* slot, drive, volume, MON flags */
            }
        } else if (!a->name2[0]) {
            /* RENAME OLD,NEW: the second name */
            n = 0;
            while (*q && *q != ',' && n < 127)
                a->name2[n++] = *q++;
            a->name2[n] = '\0';
            while (n > 0 && a->name2[n - 1] == ' ')
                a->name2[--n] = '\0';
        }
        while (*q && *q != ',')
            q++;
        p = q;
    }
}

static void need_name(const args_t *a)
{
    if (!a->name[0])
        fail(ERR_DOSSYNTAX);
}

/* Where a READ or WRITE starts: record R of length L, or byte B. */
static void position(int i, const args_t *a)
{
    long off = 0;
    if (a->hasL)
        files[i].reclen = a->L;
    if (a->hasR && files[i].reclen > 0)
        off = a->R * files[i].reclen;
    if (a->hasB)
        off += a->B;
    if (a->hasR || a->hasB)
        fseek(files[i].f, off, SEEK_SET);
}

static void catalog(void)
{
#ifdef __DOS__
    DIR *d = opendir(".");
#else
    DIR *d = opendir(".");
#endif
    struct dirent *e;
    char line[64];

    scr_puts("DISK VOLUME 254");
    scr_newline();
    scr_newline();
    if (!d)
        return;
    while ((e = readdir(d)) != 0) {
        const char *dot = strrchr(e->d_name, '.');
        char type;
        long size = 0;
        FILE *f;
        int i, n;

        if (!dot)
            continue;
        if (strcmp(dot, ".BAS") == 0 || strcmp(dot, ".bas") == 0) type = 'A';
        else if (strcmp(dot, ".TXT") == 0 || strcmp(dot, ".txt") == 0) type = 'T';
        else if (strcmp(dot, ".BIN") == 0 || strcmp(dot, ".bin") == 0) type = 'B';
        else continue;
        f = fopen(e->d_name, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            size = ftell(f);
            fclose(f);
        }
        /* DOS counted sectors of 256 bytes, plus one for the track/sector
         * list every file carried. */
        sprintf(line, " %c %03ld ", type, size / 256 + 1);
        n = (int)strlen(line);
        for (i = 0; e->d_name + i < dot && n < 60; i++)
            line[n++] = (char)toupper((unsigned char)e->d_name[i]);
        line[n] = '\0';
        scr_puts(line);
        scr_newline();
    }
    closedir(d);
}

static void bload(const args_t *a)
{
    char path[160];
    FILE *f;
    unsigned char hdr[4];
    long addr, len, i;

    need_name(a);
    dos_path(a->name, ".BIN", path, (int)sizeof(path));
    f = fopen(path, "rb");
    if (!f)
        fail(ERR_FILENOTFOUND);
    if (fread(hdr, 1, 4, f) != 4) {
        fclose(f);
        fail(ERR_IOERROR);
    }
    addr = hdr[0] | ((long)hdr[1] << 8);
    len = hdr[2] | ((long)hdr[3] << 8);
    if (a->hasA)
        addr = a->A;
    for (i = 0; i < len && addr + i < A2_MEMSIZE; i++) {
        int c = fgetc(f);
        if (c == EOF)
            break;
        a2mem[addr + i] = (unsigned char)c;
    }
    fclose(f);
    /* A picture loaded onto a page is on the screen at once. */
    gfx_notify(0);
}

static void bsave(const args_t *a)
{
    char path[160];
    FILE *f;
    unsigned char hdr[4];
    long i;

    need_name(a);
    if (!a->hasA || !a->hasL)
        fail(ERR_DOSSYNTAX);
    dos_path(a->name, ".BIN", path, (int)sizeof(path));
    f = fopen(path, "wb");
    if (!f)
        fail(ERR_WRITEPROT);
    hdr[0] = (unsigned char)(a->A & 0xFF);
    hdr[1] = (unsigned char)((a->A >> 8) & 0xFF);
    hdr[2] = (unsigned char)(a->L & 0xFF);
    hdr[3] = (unsigned char)((a->L >> 8) & 0xFF);
    fwrite(hdr, 1, 4, f);
    for (i = 0; i < a->L && a->A + i < A2_MEMSIZE; i++)
        fputc(a2mem[a->A + i], f);
    fclose(f);
}

static int verb_is(const char *v, const char *w) { return strcmp(v, w) == 0; }

/* The verb at the front of a line, upper-cased: letters, and the '#' that
 * ends PR# and IN#; what follows is the argument. */
static int take_verb(const char *text, char *verb, const char **rest)
{
    int n = 0;
    while (*text == ' ')
        text++;
    while (isalpha((unsigned char)*text) && n < 15)
        verb[n++] = (char)toupper((unsigned char)*text++);
    if (*text == '#' && n < 15)
        verb[n++] = *text++;
    verb[n] = '\0';
    *rest = text;
    return n;
}

static const char *const verbs[] = {
    "CATALOG", "OPEN", "CLOSE", "READ", "WRITE", "APPEND", "POSITION",
    "DELETE", "RENAME", "LOCK", "UNLOCK", "VERIFY", "MON", "NOMON",
    "MAXFILES", "INIT", "FP", "INT", "BLOAD", "BSAVE", "BRUN", "EXEC", "CHAIN", 0
};

int dos_is_command(const char *line)
{
    char verb[16];
    const char *rest;
    int i;
    if (!take_verb(line, verb, &rest))
        return 0;
    for (i = 0; verbs[i]; i++)
        if (verb_is(verb, verbs[i]))
            return 1;
    return 0;
}

void dos_command(const char *text)
{
    char verb[16];
    const char *rest;
    args_t a;
    char path[160];
    int i;

    /* A bare control-D: READ and WRITE off, nothing else. */
    if (!take_verb(text, verb, &rest)) {
        rd = wr = -1;
        return;
    }
    rd = wr = -1;
    parse_args(rest, &a);

    if (verb_is(verb, "CATALOG")) {
        catalog();
    } else if (verb_is(verb, "OPEN")) {
        need_name(&a);
        dos_path(a.name, ".TXT", path, (int)sizeof(path));
        i = open_file(path, a.hasL ? a.L : 0);
        if (i >= 0)
            rewind(files[i].f);
    } else if (verb_is(verb, "CLOSE")) {
        if (!a.name[0]) {
            dos_close_all();
        } else {
            dos_path(a.name, ".TXT", path, (int)sizeof(path));
            i = find_open(path);
            if (i >= 0)
                close_slot(i);
        }
    } else if (verb_is(verb, "READ")) {
        need_name(&a);
        dos_path(a.name, ".TXT", path, (int)sizeof(path));
        i = find_open(path);
        if (i < 0) {
            if (!exists(path))
                fail(ERR_FILENOTFOUND);
            i = open_file(path, a.hasL ? a.L : 0);
        }
        if (i < 0)
            return;
        position(i, &a);
        rd = i;
    } else if (verb_is(verb, "WRITE") || verb_is(verb, "APPEND")) {
        need_name(&a);
        dos_path(a.name, ".TXT", path, (int)sizeof(path));
        i = find_open(path);
        if (i < 0)
            i = open_file(path, a.hasL ? a.L : 0);
        if (i < 0)
            return;
        if (verb_is(verb, "APPEND")) {
            fseek(files[i].f, 0, SEEK_END);
        } else if (!a.hasR && !a.hasB && ftell(files[i].f) == 0) {
            /* WRITE from the top replaced what was there, sector by sector;
             * a fresh file is the honest version of that. */
            files[i].f = freopen(path, "w+", files[i].f);
            if (!files[i].f) {
                files[i].path[0] = '\0';
                fail(ERR_WRITEPROT);
            }
        } else {
            position(i, &a);
        }
        wr = i;
    } else if (verb_is(verb, "POSITION")) {
        long n;
        need_name(&a);
        dos_path(a.name, ".TXT", path, (int)sizeof(path));
        i = find_open(path);
        if (i < 0)
            fail(ERR_FILENOTFOUND);
        /* Skip R fields: a field ends at a return. */
        for (n = 0; n < a.R; n++) {
            int c;
            do { c = fgetc(files[i].f); } while (c != EOF && c != '\n' && c != '\r');
            if (c == EOF)
                fail(ERR_ENDOFDATA);
        }
    } else if (verb_is(verb, "DELETE")) {
        need_name(&a);
        dos_path(a.name, ".TXT", path, (int)sizeof(path));
        i = find_open(path);
        if (i >= 0)
            close_slot(i);
        if (!exists(path)) {
            dos_path(a.name, ".BAS", path, (int)sizeof(path));
            if (!exists(path)) {
                dos_path(a.name, ".BIN", path, (int)sizeof(path));
                if (!exists(path))
                    fail(ERR_FILENOTFOUND);
            }
        }
        remove(path);
    } else if (verb_is(verb, "RENAME")) {
        char path2[160];
        const char *ext = ".TXT";
        need_name(&a);
        if (!a.name2[0])
            fail(ERR_DOSSYNTAX);
        dos_path(a.name, ".TXT", path, (int)sizeof(path));
        if (!exists(path)) {
            ext = ".BAS";
            dos_path(a.name, ext, path, (int)sizeof(path));
            if (!exists(path)) {
                ext = ".BIN";
                dos_path(a.name, ext, path, (int)sizeof(path));
                if (!exists(path))
                    fail(ERR_FILENOTFOUND);
            }
        }
        dos_path(a.name2, ext, path2, (int)sizeof(path2));
        rename(path, path2);
    } else if (verb_is(verb, "LOCK") || verb_is(verb, "UNLOCK") || verb_is(verb, "VERIFY")) {
        need_name(&a);
        dos_path(a.name, ".TXT", path, (int)sizeof(path));
        if (!exists(path)) {
            dos_path(a.name, ".BAS", path, (int)sizeof(path));
            if (!exists(path)) {
                dos_path(a.name, ".BIN", path, (int)sizeof(path));
                if (!exists(path))
                    fail(ERR_FILENOTFOUND);
            }
        }
    } else if (verb_is(verb, "MON") || verb_is(verb, "NOMON") || verb_is(verb, "MAXFILES") ||
               verb_is(verb, "INIT") || verb_is(verb, "FP") || verb_is(verb, "INT")) {
        /* Nothing here is a floppy. */
    } else if (verb_is(verb, "LOAD") || verb_is(verb, "RUN")) {
        need_name(&a);
        dos_path(a.name, ".BAS", path, (int)sizeof(path));
        if (!exists(path))
            fail(ERR_FILENOTFOUND);
        if (program_hook)
            program_hook(path, verb_is(verb, "RUN"));
    } else if (verb_is(verb, "SAVE")) {
        need_name(&a);
        dos_path(a.name, ".BAS", path, (int)sizeof(path));
        if (!it_save(path))
            fail(ERR_WRITEPROT);
    } else if (verb_is(verb, "BLOAD")) {
        bload(&a);
    } else if (verb_is(verb, "BSAVE")) {
        bsave(&a);
    } else if (verb_is(verb, "PR#")) {
        long slot = number(rest);
        if (slot == 1) printer_on();
        else if (slot == 3) { printer_off(); scr_set_cols(80); }
        else if (slot == 0) { printer_off(); scr_set_cols(40); }
    } else if (verb_is(verb, "IN#")) {
        /* the keyboard is the only input here */
    } else {
        /* BRUN, EXEC and CHAIN included: a 6502 to run, lines to type, a
         * program to swap under running variables. None of those are here. */
        fail(ERR_DOSSYNTAX);
    }
}
