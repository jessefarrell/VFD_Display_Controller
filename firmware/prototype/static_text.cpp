#include "static_text.h"
#include "app_common.h"
#include "settings.h"
#include <cstdio>
#include <cstring>

static_assert(STATIC_TEXT_MAX_LEN == Vfd::MAX_DIGITS);

namespace {
void render(Vfd& vfd) {
    vfd.cursor_blink(false);
    vfd.clear_screen();
    vfd.cursor_move(0);
    vfd.write_string(settings().static_text);
}

void edit_text(Vfd& vfd) {
    std::printf("Static text (max %u characters; Enter keeps \"%s\"): ",
                unsigned(STATIC_TEXT_MAX_LEN), settings().static_text);
    char entered[STATIC_TEXT_MAX_LEN + 1];
    if (!read_text_line(entered, sizeof(entered)) || entered[0] == '\0') return;
    if (std::strcmp(entered, settings().static_text) == 0) return;
    std::strcpy(settings().static_text, entered);
    settings_save();
    render(vfd);
}
}

void run_static_text_app(Vfd& vfd, bool resume) {
    std::printf("\n--- Static Text ---\n");
    render(vfd);
    if (!resume) edit_text(vfd);
    std::printf("Press E to edit, C to clear. Esc / Ctrl+C returns to the launcher.\n");
    std::printf("Esc / Ctrl+C during editing cancels the edit.\n");
    while (true) {
        int key = poll_char();
        if (key >= 0) {
            char c = static_cast<char>(key);
            if (is_exit_key(c)) {
                std::printf("-> Exiting Static Text, returning to menu\n");
                return;
            }
            if (c == 'e' || c == 'E') {
                edit_text(vfd);
            } else if (c == 'c' || c == 'C') {
                if (settings().static_text[0] != '\0') {
                    settings().static_text[0] = '\0';
                    settings_save();
                    render(vfd);
                }
            }
        }
        // The display remains unchanged while waiting for commands.
        wait_for_input(1000);
    }
}
