#ifndef ANIMATION_H
#define ANIMATION_H

#include "vfd.h"

/**
 * @brief Runs an idle-animation showcase on the VFD.
 *
 * On entry, prints a menu of the built-in effects and blocks for one
 * keypress: '1'-'5' locks playback to that single effect, played on
 * repeat for up to an hour at a stretch. '0' (or any unrecognized key)
 * plays effects at random, each for a "shuffle time" you're then prompted
 * to enter in seconds (Enter keeps the current/default value). Effects
 * are time-based, not frame-count-based: whichever duration applies, the
 * effect keeps looping its pattern until that time is up.
 *
 * Once running, effects play back-to-back automatically -- no input
 * needed between them. The app also polls (non-blocking) for a key
 * between effects: a digit switches the running selection live
 * (re-prompting for shuffle time if you switch to '0'), Ctrl+A reprints
 * the menu, and Esc/Ctrl+C exits back to main.cpp's menu -- mid-effect or
 * between effects either way.
 *
 * Selecting Marquee explicitly (not via Random) additionally prompts for
 * the message to display and the per-character update rate, in
 * milliseconds -- how often a new character is pulled in on one edge and
 * pushed out the other. The message can be longer than the 20-character
 * display; it scrolls through as a sliding window. Settings persist
 * (Enter at either prompt keeps the current value) until changed again.
 *
 * @param vfd - reference to an already-initialized Vfd instance.
 */
void run_animation_app(Vfd& vfd);

#endif /* ANIMATION_H */
