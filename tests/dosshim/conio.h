#ifndef DOSSHIM_CONIO_H
#define DOSSHIM_CONIO_H
int kbhit(void);
int getch(void);
int ungetch(int c);
void outp(unsigned port, int value);
/* Keys the next getch calls will return. */
void shim_type(const char *s);
int  shim_keys_left(void);
#endif
