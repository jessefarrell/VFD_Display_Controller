#include "app_common.h"

bool exit_requested(void) {
    int c = getchar_timeout_us(0);  // poll only, never block
    if (c == PICO_ERROR_TIMEOUT) {
        return false;
    }
    return is_exit_key((char)c);
}

bool sleep_or_exit(uint32_t duration_ms) {
    const uint32_t step_ms = 50;
    uint32_t elapsed = 0;

    while (elapsed < duration_ms) {
        if (exit_requested()) {
            return true;
        }
        uint32_t this_step = (duration_ms - elapsed < step_ms) ? (duration_ms - elapsed) : step_ms;
        sleep_ms(this_step);
        elapsed += this_step;
    }
    return false;
}
