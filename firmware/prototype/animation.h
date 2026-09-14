#ifndef ANIMATION_H
#define ANIMATION_H

#include "vfd.h"

/**
 * @brief Runs an idle-animation showcase on the VFD.
 *
 * Repeatedly picks one of several built-in animation effects at random
 * and plays it to completion, then picks another. Purely decorative --
 * intended as a "screensaver" style app selectable from the menu in
 * main.cpp. Returns when the user presses Esc or Ctrl+C, so main.cpp can
 * return to its menu.
 *
 * @param vfd - reference to an already-initialized Vfd instance.
 */
void run_animation_app(Vfd& vfd);

#endif /* ANIMATION_H */
