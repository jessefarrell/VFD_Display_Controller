#ifndef TEST_DISPLAY_H
#define TEST_DISPLAY_H

#include "vfd.h"

/**
 * @brief Runs the interactive VFD class test harness.
 *
 * Reads characters from stdin and maps them to Vfd method calls so
 * individual display commands can be exercised by hand over serial.
 * Returns when the user presses Esc or Ctrl+C, so main.cpp can return to
 * its menu.
 *
 * @param vfd - reference to an already-initialized Vfd instance.
 */
void run_display_test_app(Vfd& vfd);

#endif /* TEST_DISPLAY_H */
