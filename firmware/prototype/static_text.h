#ifndef STATIC_TEXT_H
#define STATIC_TEXT_H

#include "vfd.h"

// Displays a saved, non-scrolling line. On manual selection, prompts for text;
// automatic resume skips the prompt. E edits, C clears, Esc/Ctrl+C exits.
void run_static_text_app(Vfd& vfd, bool resume = false);

#endif
