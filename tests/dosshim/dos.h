/* Stand-in for Watcom's dos.h, so the DOS front end compiles on the host
 * and its video layer can be checked: int86 records mode sets and cursor
 * moves, MK_FP hands out arrays in place of video memory. */
#ifndef DOSSHIM_DOS_H
#define DOSSHIM_DOS_H
union REGS {
    struct { unsigned short ax, bx, cx, dx, si, di, cflag; } x;
    struct { unsigned char al, ah, bl, bh, cl, ch, dl, dh; } h;
};
int int86(int intno, union REGS *in, union REGS *out);
void *shim_mem(unsigned seg);
#define MK_FP(seg, off) ((void *)((char *)shim_mem(seg) + (off)))

/* What the shim saw. */
extern int shim_video_mode;          /* last mode set through int 10h */
extern int shim_mode_sets;
extern int shim_cursor_row, shim_cursor_col;
extern unsigned char shim_vga[64000];
extern unsigned char shim_text[8000];
extern unsigned char shim_dac[256][3];
#endif
