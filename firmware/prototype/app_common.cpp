#include "app_common.h"
#include <cstdio>

namespace {
int pending_char = -1;
bool skip_lf = false;

int read_normalized_char(uint32_t timeout_us) {
    while (true) {
        int c = getchar_timeout_us(timeout_us);
        if (c == PICO_ERROR_TIMEOUT) return -1;
        bool paired_lf = skip_lf && c == '\n';
        skip_lf = c == '\r';
        if (paired_lf) continue;
        return c == '\r' ? '\n' : c;
    }
}
}

int poll_char(void) {
    if (pending_char >= 0) {
        int c = pending_char;
        pending_char = -1;
        return c;
    }
    return read_normalized_char(0);
}

int peek_char(void) {
    if (pending_char < 0) pending_char = read_normalized_char(0);
    return pending_char;
}

char read_char(void) {
    int c = poll_char();
    // UINT32_MAX is a finite SDK timeout. Retry if it expires so a long
    // idle session cannot inject a synthetic NUL into an input prompt.
    while (c < 0) c = read_normalized_char(UINT32_MAX);
    std::putchar(c);
    return static_cast<char>(c);
}

bool wait_for_input(uint32_t duration_ms) {
    const uint32_t step_ms = 50;
    uint32_t elapsed = 0;

    while (elapsed < duration_ms) {
        if (peek_char() >= 0) return true;
        uint32_t this_step = (duration_ms - elapsed < step_ms) ? (duration_ms - elapsed) : step_ms;
        sleep_ms(this_step);
        elapsed += this_step;
    }
    return peek_char() >= 0;
}

bool read_text_line(char* buf, size_t buf_size) {
    if (buf_size == 0) return false;
    size_t len = 0;
    buf[0] = '\0';
    bool saw_newline = false;
    bool separator_pending = false;
    bool overflow = false;
    auto append = [&](char c) {
        if (len + 1 < buf_size) {
            buf[len++] = c;
            buf[len] = '\0';
        } else {
            overflow = true;
        }
    };

    while (true) {
        if (saw_newline && !wait_for_input(250)) break;
        char c = read_char();
        if (is_exit_key(c)) {
            std::printf("\n");
            return false;
        }
        if (c == '\n') {
            saw_newline = true;
            separator_pending = len > 0;
            continue;
        }
        if ((c == 0x7f || c == 0x08) && len > 0) {
            buf[--len] = '\0';
            separator_pending = false;
            std::printf(" \b");
            continue;
        }
        if (c == '\t') c = ' ';
        if (c >= 0x20 && c < 0x7f) {
            if (separator_pending && c != ' ' && len > 0 && buf[len - 1] != ' ') append(' ');
            separator_pending = false;
            append(c);
        }
    }
    std::printf("\n");
    if (overflow) {
        std::printf("-> Message exceeds %u characters; previous message retained\n", unsigned(buf_size - 1));
        return false;
    }
    return true;
}
