#ifndef APP_COMMON_H
#define APP_COMMON_H

#include "pico/stdlib.h"
#include <cstddef>
#include <cstdint>

// ESC and Ctrl+C are the bytes a terminal sends for the Escape key and
// Ctrl+C respectively. Every app treats either one as "return to menu".
constexpr char KEY_ESC    = 0x1B;
constexpr char KEY_CTRL_C = 0x03;

inline bool is_exit_key(char c) {
    return c == KEY_ESC || c == KEY_CTRL_C;
}

// All console reads share CR/LF normalization, including across app changes.
// CR and LF each produce '\n'; a CRLF pair produces just one '\n', even
// when its bytes arrive in separate USB packets.

// Consumes one character without blocking, or returns -1 if none is available.
int poll_char(void);

// Returns the next character without consuming it, or -1 if none is available.
int peek_char(void);

// Blocks until a character is available, then consumes and echoes it.
char read_char(void);

// Sleeps for up to duration_ms, checking for input at most every 50 ms.
// Returns true if input is waiting; leaves it queued for the app to handle.
// Callers must return to their input loop, rather than treating this as an exit.
bool wait_for_input(uint32_t duration_ms);

// Reads printable text into a work buffer. After the first newline, collects
// until input is idle for 250 ms and joins pasted lines/tabs with spaces.
// False on Esc/Ctrl+C or oversized input; oversized pastes are fully consumed.
// The caller must keep its saved text separate until this returns true.
bool read_text_line(char* buf, size_t buf_size);

#endif /* APP_COMMON_H */
