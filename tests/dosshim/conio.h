#ifndef DOSSHIM_CONIO_H
#define DOSSHIM_CONIO_H
int kbhit(void);
int getch(void);
int ungetch(int c);
void outp(unsigned port, int value);
/* Keys the next getch calls will return. */
void shim_type(const char *s);
int  shim_keys_left(void);
/* Called when the program asks for a key and none is scripted; the
 * harness uses it to snapshot the screen that is waiting, then type. */
void shim_on_empty(void (*fn)(void));
#endif
