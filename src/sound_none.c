/* No speaker: the host build. A transcript with bells in it would only
 * confuse the tests that diff transcripts, so the bell is silent too. */
#include "sound.h"

void snd_click(void) { }
void snd_bell(void) { }
