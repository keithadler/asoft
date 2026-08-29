/* panes.h - the text shown in the Turbo Vision side panes.
 *
 * Kept separate from the Turbo Vision code, and portable, because the
 * interesting part of those panes is what they say about the interpreter, not
 * how they are drawn. The front end frames these lines; tests check them.
 *
 * The layout follows web/ui-mockup.html, whose numbers turn out to be
 * self-consistent with the frame sizes measured from the reference build: the
 * mockup's "Control stack 52/240" is one FOR (18) plus one GOSUB (6) plus
 * seven leaked ONERR frames (4 each).
 */
#ifndef ASOFT_PANES_H
#define ASOFT_PANES_H

#define PANE_WIDTH 35
#define PANE_MAXLINES 32

/* Each line comes with a hint so the front end can colour it the way the
 * mockup does without parsing the text back. */
#define PL_PLAIN   0
#define PL_HEADING 1   /* cyan section heading */
#define PL_WARN    2   /* yellow: a leak, or a bug that is switched on */
#define PL_RULE    3   /* horizontal rule */

typedef struct {
    char text[PANE_WIDTH + 1];
    int  style;
} pane_line;

/* Fill out with the Machine pane's contents. Returns the number of lines. */
int pane_machine(pane_line *out, int max);

#endif
