#include "bugs.h"

/* All on: the default is the ROM, warts included. */
unsigned char bug_enabled[BUG_COUNT] = { 1, 1, 1, 1 };

static const char *const names[BUG_COUNT] = {
    "ONERR leak",
    "HTAB off-by-two",
    "MBF rounding",
    "Greedy tokenizer"
};

const char *bug_name(int which)
{
    if (which < 0 || which >= BUG_COUNT)
        return "?";
    return names[which];
}
