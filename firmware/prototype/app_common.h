#ifndef APP_COMMON_H
#define APP_COMMON_H

#include "pico/stdlib.h"
#include <cstdint>

// ESC and Ctrl+C are the bytes a terminal sends for the Escape key and
// Ctrl+C respectively. Every app treats either one as "return to menu".
constexpr char KEY_ESC    = 0x1B;
constexpr char KEY_CTRL_C = 0x03;

inline bool is_exit_key(char c) {
    return c == KEY_ESC || c == KEY_CTRL_C;
}

/**
 * @brief Non-blocking check for a single byte on stdin.
 *
 * Intended for apps that mostly run "hands off" but still want to react
 * to an occasional keypress without blocking their main loop (e.g. the
 * animation app switching effects mid-run).
 *
 * @return the character read, or -1 if nothing was waiting.
 */
int poll_char(void);

/**
 * @brief Non-blocking check for an exit request (ESC or Ctrl+C) on stdin.
 *
 * Intended for apps that don't otherwise read input (clock): call once
 * per loop iteration. Any other incoming byte is silently discarded,
 * since those apps have no other use for keyboard input. Built on
 * poll_char(); apps that need to react to other keys too (e.g.
 * animation) should call poll_char() directly instead.
 *
 * @return true if the user just requested to return to the menu.
 */
bool exit_requested(void);

/**
 * @brief Sleeps for the given duration, polling for an exit request every
 * ~50ms so a long wait (e.g. the clock's 1-second refresh) doesn't leave
 * ESC/Ctrl+C feeling unresponsive.
 *
 * @param duration_ms - total time to sleep, in milliseconds.
 * @return true if an exit was requested during the sleep (the sleep is
 * cut short in that case, so callers should stop and return immediately).
 */
bool sleep_or_exit(uint32_t duration_ms);

#endif /* APP_COMMON_H */
