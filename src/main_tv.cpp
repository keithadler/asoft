/* main_tv.cpp - the Turbo Vision front end.
 *
 * The layout is web/ui-mockup.html: an Apple ][ window holding the 40x24
 * screen, a Machine pane showing the zero-page pointers and the control
 * stack, a Program pane listing the tokenised program, a menu bar and a
 * status line. That does not fit in 80x25, so the application switches to the
 * 8x8 font (80x43) on startup.
 *
 * Everything below is presentation. The interpreter, the memory image and the
 * pane contents are the same portable C the console build uses, so the two
 * front ends cannot disagree about what a program does -- only about where
 * the characters land.
 *
 * BUILD: this needs Borland C++ 3.1 and its Turbo Vision library, running
 * under DOS or DOSBox; see makefile.bc and the README. It is the one part of
 * this repository that has NOT been compiled or run -- there is no 16-bit
 * Borland toolchain on the machine it was written on, and Turbo Vision does
 * not build under Open Watcom. Treat it as unverified until it has been
 * through bcc once.
 */

#define Uses_TApplication
#define Uses_TDeskTop
#define Uses_TDialog
#define Uses_TDrawBuffer
#define Uses_TEvent
#define Uses_TFileDialog
#define Uses_TKeys
#define Uses_TMenuBar
#define Uses_TMenuItem
#define Uses_TRect
#define Uses_TScreen
#define Uses_TScrollBar
#define Uses_TStaticText
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TStatusLine
#define Uses_TSubMenu
#define Uses_TView
#define Uses_TWindow
#define Uses_MsgBox
#include <tv.h>

#include <string.h>
#include <stdio.h>

extern "C" {
#include "a2mem.h"
#include "bugs.h"
#include "errs.h"
#include "host.h"
#include "interp.h"
#include "panes.h"
#include "screen.h"
#include "token.h"
}

const int cmRunProgram   = 1000;
const int cmBreakProgram = 1001;
const int cmLoadProgram  = 1002;
const int cmSaveProgram  = 1003;
const int cmNewProgram   = 1004;
const int cmAboutBox     = 1005;
const int cmToggleBug    = 1010;   /* + bug index */
const int cmClearVars    = 1020;
const int cmPopOnerr     = 1021;

/* ------------------------------------------------------- the Apple screen */

/* A 40x24 character buffer plus the line the user is typing. The interpreter
 * writes into it through the same screen model the console build uses, so
 * wrapping and POS behave identically. */
static const int APPLE_W = SCR_COLS;
static const int APPLE_H = SCR_ROWS;

class TAppleView : public TView {
public:
    TAppleView(const TRect &bounds);
    virtual void draw();
    virtual void handleEvent(TEvent &event);

    void putch(char ch);
    void clear();

    /* Pumps events until the user completes a line; used by INPUT and by the
     * main REPL. Returns 0 if the application is closing. */
    int readLine(char *buf, int max);
    int readKey();
    int breakPressed();

private:
    char cells[APPLE_H][APPLE_W];
    int  row;
    char input[256];
    int  inputLen;
    int  inputReady;
    int  pendingKey;
    int  breakSeen;
    int  closing;

    void scrollUp();
};

static TAppleView *appleView = 0;

TAppleView::TAppleView(const TRect &bounds) : TView(bounds)
{
    growMode = gfGrowHiX | gfGrowHiY;
    options |= ofSelectable;
    row = 0;
    inputLen = inputReady = pendingKey = breakSeen = closing = 0;
    clear();
}

void TAppleView::clear()
{
    for (int r = 0; r < APPLE_H; r++)
        memset(cells[r], ' ', APPLE_W);
    row = 0;
    drawView();
}

void TAppleView::scrollUp()
{
    for (int r = 0; r < APPLE_H - 1; r++)
        memcpy(cells[r], cells[r + 1], APPLE_W);
    memset(cells[APPLE_H - 1], ' ', APPLE_W);
}

/* The screen model has already decided the column and issued any wrap, so
 * this only has to place the character and follow the newlines. */
void TAppleView::putch(char ch)
{
    if (ch == '\f') { clear(); return; }
    if (ch == '\n') {
        if (row < APPLE_H - 1)
            row++;
        else
            scrollUp();
        drawView();
        return;
    }
    int col = scr_col() - 1;
    if (col < 0)
        col = 0;
    if (col >= APPLE_W)
        col = APPLE_W - 1;
    cells[row][col] = ch;
    drawView();
}

void TAppleView::draw()
{
    TDrawBuffer b;
    ushort colour = getColor(0x0301);        /* green on black, Apple-ish */
    for (int y = 0; y < size.y; y++) {
        b.moveChar(0, ' ', colour, size.x);
        if (y < APPLE_H) {
            for (int x = 0; x < size.x && x < APPLE_W; x++)
                b.moveChar(x, cells[y][x], colour, 1);
            /* the cursor block, on the line being typed */
            if (y == row) {
                int c = scr_col();
                if (c < size.x)
                    b.moveChar(c, '_', colour, 1);
            }
        }
        writeLine(0, y, size.x, 1, b);
    }
}

void TAppleView::handleEvent(TEvent &event)
{
    TView::handleEvent(event);
    if (event.what != evKeyDown)
        return;

    ushort code = event.keyDown.keyCode;
    char ch = event.keyDown.charScan.charCode;

    if (code == kbCtrlC) {
        breakSeen = 1;
        clearEvent(event);
        return;
    }
    if (code == kbEnter) {
        input[inputLen] = '\0';
        inputReady = 1;
        clearEvent(event);
        return;
    }
    if (code == kbBack) {
        if (inputLen > 0) {
            inputLen--;
            /* rub the character out on screen too */
            int col = scr_col();
            if (col > 0) {
                cells[row][col - 1] = ' ';
                drawView();
            }
        }
        clearEvent(event);
        return;
    }
    if (ch >= ' ' && ch < 127 && inputLen < (int)sizeof(input) - 1) {
        input[inputLen++] = ch;
        pendingKey = (unsigned char)ch;
        scr_putc(ch);                 /* echo through the screen model */
        clearEvent(event);
    }
}

int TAppleView::readLine(char *buf, int max)
{
    inputLen = 0;
    inputReady = 0;
    while (!inputReady && !closing) {
        TEvent e;
        TProgram::application->getEvent(e);
        TProgram::application->handleEvent(e);
        if (TProgram::application->endState != 0)
            closing = 1;
    }
    if (closing)
        return 0;
    strncpy(buf, input, (size_t)max - 1);
    buf[max - 1] = '\0';
    inputLen = 0;
    inputReady = 0;
    scr_newline();
    return 1;
}

int TAppleView::readKey()
{
    pendingKey = 0;
    while (!pendingKey && !closing) {
        TEvent e;
        TProgram::application->getEvent(e);
        TProgram::application->handleEvent(e);
        if (TProgram::application->endState != 0)
            closing = 1;
    }
    int k = pendingKey;
    pendingKey = 0;
    return closing ? 0 : k;
}

int TAppleView::breakPressed()
{
    /* Poll without blocking, so a long FOR loop can still be interrupted. */
    TEvent e;
    TProgram::application->idle();
    if (breakSeen) { breakSeen = 0; return 1; }
    return 0;
}

/* ------------------------------------------------------------- side panes */

class TMachineView : public TView {
public:
    TMachineView(const TRect &bounds) : TView(bounds)
        { growMode = gfGrowHiY | gfGrowLoX | gfGrowHiX; }
    virtual void draw();
};

void TMachineView::draw()
{
    pane_line pl[PANE_MAXLINES];
    int n = pane_machine(pl, PANE_MAXLINES);
    TDrawBuffer b;
    ushort plain = getColor(0x0301);
    ushort head  = getColor(0x0303);
    ushort warn  = getColor(0x0302);

    for (int y = 0; y < size.y; y++) {
        b.moveChar(0, ' ', plain, size.x);
        if (y < n) {
            if (pl[y].style == PL_RULE) {
                b.moveChar(0, '\304', plain, size.x);
            } else {
                ushort c = pl[y].style == PL_HEADING ? head :
                           pl[y].style == PL_WARN ? warn : plain;
                b.moveStr(1, pl[y].text, c);
            }
        }
        writeLine(0, y, size.x, 1, b);
    }
}

class TProgramView : public TView {
public:
    TProgramView(const TRect &bounds) : TView(bounds), top(0)
        { growMode = gfGrowHiX | gfGrowHiY; options |= ofSelectable; }
    virtual void draw();
    virtual void handleEvent(TEvent &event);
private:
    int top;
};

void TProgramView::draw()
{
    TDrawBuffer b;
    ushort colour = getColor(0x0301);
    char text[512], line[256];
    a2addr p = a2_prog_first();

    for (int skip = 0; skip < top && p; skip++)
        p = a2_prog_next(p);

    for (int y = 0; y < size.y; y++) {
        b.moveChar(0, ' ', colour, size.x);
        if (p) {
            tok_detokenize(a2_prog_tokens(p), text, (int)sizeof(text));
            sprintf(line, "%d %s", a2_prog_lineno(p), text);
            line[sizeof(line) - 1] = '\0';
            b.moveStr(1, line, colour);
            p = a2_prog_next(p);
        }
        writeLine(0, y, size.x, 1, b);
    }
}

void TProgramView::handleEvent(TEvent &event)
{
    TView::handleEvent(event);
    if (event.what == evKeyDown) {
        if (event.keyDown.keyCode == kbDown) { top++; drawView(); clearEvent(event); }
        else if (event.keyDown.keyCode == kbUp && top > 0) { top--; drawView(); clearEvent(event); }
    }
}

static TMachineView *machineView = 0;
static TProgramView *programView = 0;

/* A window that will not go away, since the layout is fixed. */
class TFixedWindow : public TWindow {
public:
    TFixedWindow(const TRect &b, const char *t, short n) : TWindowInit(&TFixedWindow::initFrame),
        TWindow(b, t, n) { flags &= ~(wfClose | wfZoom); }
};

/* ---------------------------------------------------------- the host hooks */

extern "C" {

static void tv_sink(char ch)
{
    if (appleView)
        appleView->putch(ch);
}

int host_getline(char *buf, int max)
{
    return appleView ? appleView->readLine(buf, max) : 0;
}

int host_getkey(void)
{
    return appleView ? appleView->readKey() : 0;
}

int host_break(void)
{
    return appleView ? appleView->breakPressed() : 0;
}

}

/* ------------------------------------------------------------ application */

class TAsoftApp : public TApplication {
public:
    TAsoftApp();
    virtual void handleEvent(TEvent &event);
    static TMenuBar *initMenuBar(TRect r);
    static TStatusLine *initStatusLine(TRect r);
    void run();
private:
    void refreshPanes();
    void runProgram();
    void loadProgram();
    void saveProgram();
    void aboutBox();
};

TAsoftApp::TAsoftApp()
    : TProgInit(&TAsoftApp::initStatusLine,
                &TAsoftApp::initMenuBar,
                &TAsoftApp::initDeskTop)
{
    /* 80x43. A 40-column Apple screen plus a frame, a menu bar, a status line
     * and two side panes will not fit in 80x25. */
    setScreenMode(smFont8x8);

    TRect r = getExtent();
    int appleW = APPLE_W + 2;
    int topH = APPLE_H + 2;

    TWindow *w = new TFixedWindow(TRect(0, 1, appleW, 1 + topH), "Apple ][", 1);
    appleView = new TAppleView(w->getExtent().grow(-1, -1));
    w->insert(appleView);
    deskTop->insert(w);

    TWindow *m = new TFixedWindow(TRect(appleW, 1, r.b.x, 1 + topH), "Machine", 2);
    machineView = new TMachineView(m->getExtent().grow(-1, -1));
    m->insert(machineView);
    deskTop->insert(m);

    TWindow *p = new TFixedWindow(TRect(0, 1 + topH, r.b.x, r.b.y - 1), "Program", 3);
    programView = new TProgramView(p->getExtent().grow(-1, -1));
    p->insert(programView);
    deskTop->insert(p);

    scr_init(tv_sink);
    it_init();
}

TMenuBar *TAsoftApp::initMenuBar(TRect r)
{
    r.b.y = r.a.y + 1;
    return new TMenuBar(r,
        *new TSubMenu("~F~ile", kbAltF) +
            *new TMenuItem("~L~oad...", cmLoadProgram, kbF3, hcNoContext, "F3") +
            *new TMenuItem("~S~ave...", cmSaveProgram, kbF2, hcNoContext, "F2") +
            newLine() +
            *new TMenuItem("~N~ew", cmNewProgram, kbNoKey) +
            newLine() +
            *new TMenuItem("E~x~it", cmQuit, kbAltX, hcNoContext, "Alt-X") +
        *new TSubMenu("R~u~n", kbAltU) +
            *new TMenuItem("~R~un", cmRunProgram, kbCtrlR, hcNoContext, "Ctrl-R") +
            *new TMenuItem("~B~reak", cmBreakProgram, kbCtrlC, hcNoContext, "Ctrl-C") +
        *new TSubMenu("B~u~gs", kbAltU) +
            *new TMenuItem("~O~NERR leak", cmToggleBug + BUG_ONERR_LEAK, kbNoKey) +
            *new TMenuItem("~M~BF rounding", cmToggleBug + BUG_MBF_ROUNDING, kbNoKey) +
            *new TMenuItem("~G~reedy tokenizer", cmToggleBug + BUG_GREEDY_TOKENIZER, kbNoKey) +
        *new TSubMenu("D~e~bug", kbAltE) +
            *new TMenuItem("~C~lear variables", cmClearVars, kbNoKey) +
            *new TMenuItem("~P~op leaked ONERR frame", cmPopOnerr, kbNoKey) +
        *new TSubMenu("H~e~lp", kbAltE) +
            *new TMenuItem("~A~bout", cmAboutBox, kbNoKey)
        );
}

TStatusLine *TAsoftApp::initStatusLine(TRect r)
{
    r.a.y = r.b.y - 1;
    return new TStatusLine(r,
        *new TStatusDef(0, 0xFFFF) +
            *new TStatusItem("~Ctrl-R~ Run", kbCtrlR, cmRunProgram) +
            *new TStatusItem("~Ctrl-C~ Break", kbCtrlC, cmBreakProgram) +
            *new TStatusItem("~F3~ Load", kbF3, cmLoadProgram) +
            *new TStatusItem("~F2~ Save", kbF2, cmSaveProgram) +
            *new TStatusItem("~Alt-X~ Exit", kbAltX, cmQuit)
        );
}

void TAsoftApp::refreshPanes()
{
    if (machineView) machineView->drawView();
    if (programView) programView->drawView();
}

void TAsoftApp::runProgram()
{
    it_line("RUN");
    refreshPanes();
}

void TAsoftApp::loadProgram()
{
    TFileDialog *d = new TFileDialog("*.BAS", "Load program",
                                     "~N~ame", fdOpenButton, 100);
    if (deskTop->execView(d) != cmCancel) {
        char path[128];
        d->getFileName(path);
        if (!it_load(path))
            messageBox("Cannot open that file.", mfError | mfOKButton);
        refreshPanes();
    }
    destroy(d);
}

void TAsoftApp::saveProgram()
{
    TFileDialog *d = new TFileDialog("*.BAS", "Save program",
                                     "~N~ame", fdOKButton, 101);
    if (deskTop->execView(d) != cmCancel) {
        char path[128];
        d->getFileName(path);
        if (!it_save(path))
            messageBox("Cannot write that file.", mfError | mfOKButton);
    }
    destroy(d);
}

void TAsoftApp::aboutBox()
{
    messageBox("\003Applesoft BASIC for DOS\n\n"
               "\003The ROM's bugs, on purpose.\n"
               "\003Switch them off under Bugs.",
               mfInformation | mfOKButton);
}

void TAsoftApp::handleEvent(TEvent &event)
{
    TApplication::handleEvent(event);
    if (event.what != evCommand)
        return;

    ushort c = event.message.command;

    if (c >= cmToggleBug && c < cmToggleBug + BUG_COUNT) {
        int which = c - cmToggleBug;
        bug_enabled[which] = (unsigned char)!bug_enabled[which];
        refreshPanes();
        clearEvent(event);
        return;
    }

    switch (c) {
    case cmRunProgram:  runProgram(); break;
    case cmLoadProgram: loadProgram(); break;
    case cmSaveProgram: saveProgram(); break;
    case cmNewProgram:  it_line("NEW"); refreshPanes(); break;
    case cmClearVars:   it_line("CLEAR"); refreshPanes(); break;
    case cmPopOnerr:    it_line("CALL -3288"); refreshPanes(); break;
    case cmAboutBox:    aboutBox(); break;
    default:            return;
    }
    clearEvent(event);
}

/* The REPL, driven from inside the event loop: readLine pumps events, so the
 * menus and the panes stay live while the prompt is waiting. */
void TAsoftApp::run()
{
    char line[512];

    scr_puts("APPLESOFT BASIC FOR DOS");
    scr_newline();
    scr_newline();

    while (!it_quitting()) {
        scr_raw_puts("]");
        if (!appleView->readLine(line, (int)sizeof(line)))
            break;
        it_line(line);
        refreshPanes();
    }
}

int main(int argc, char **argv)
{
    TAsoftApp app;

    if (argc > 1 && !it_load(argv[1]))
        messageBox("Cannot open that file.", mfError | mfOKButton);

    app.run();
    return 0;
}
