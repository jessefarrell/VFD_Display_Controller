/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "config.h"
#include "vfd.h"
#include "clock.h"
#include "test_display.h"
#include "animation.h"
#include "static_text.h"
#include "settings.h"
#include "app_common.h"
#include <stdio.h>

// Single global instance, shared by whichever app the user selects below.
Vfd vfd(SETUP_HOLD_TIME_US);

static void print_app_menu(void) {
    printf("\n--- VFD Launcher ---\n");
    printf("  [1] Clock\n");
    printf("  [2] Display Test Harness\n");
    printf("  [3] Animation\n");
    printf("  [4] Static Text\n");
    printf("  [R] Factory reset\n");
    printf("Select an application or command: ");
}

static bool confirm_factory_reset(void) {
    printf("\nFactory reset clears all saved settings, including startup app, animations, cursor blink, and clock date/time.\n");
    printf("Type RESET then Enter to confirm; Esc / Ctrl+C cancels: ");
    char confirmation[6] = {};
    size_t len = 0;
    bool overflow = false;
    while (true) {
        char c = read_char();
        if (is_exit_key(c)) return false;
        if (c == '\n') {
            // Ignore the Enter sometimes sent with the menu's R key.
            if (len == 0 && !overflow) continue;
            printf("\n");
            return !overflow && len == 5 &&
                   confirmation[0] == 'R' && confirmation[1] == 'E' &&
                   confirmation[2] == 'S' && confirmation[3] == 'E' && confirmation[4] == 'T';
        }
        if ((c == 0x08 || c == 0x7f) && len > 0) {
            confirmation[--len] = '\0';
            printf(" \b");
        } else if (len < sizeof(confirmation) - 1) {
            confirmation[len++] = c;
        } else {
            overflow = true;
        }
    }
}

int main() {
    stdio_init_all();
    sleep_ms(2000);  // give USB serial time to enumerate before first prints

    vfd.init();

    bool restored = settings_load();
    printf(restored ? "-> Saved settings restored\n" : "-> Using default settings\n");
    AppId next_app = settings().startup_app;
    bool resume = next_app != AppId::MENU;
    // A queued Esc/Ctrl+C during USB startup bypasses automatic resume.
    int startup_key = peek_char();
    if (startup_key >= 0 && is_exit_key(static_cast<char>(startup_key))) {
        poll_char();
        next_app = AppId::MENU;
        resume = false;
    }

    // Redisplay the menu every time an app exits (Esc/Ctrl+C) or an
    // invalid key was pressed.
    while (true) {
        char choice;
        if (next_app == AppId::MENU) {
            print_app_menu();
            choice = read_char();
            printf("\n");
        } else {
            choice = '0' + static_cast<uint8_t>(next_app);
            printf("-> Resuming saved application\n");
        }

        if (choice >= '1' && choice <= '4') {
            settings().startup_app = static_cast<AppId>(choice - '0');
            settings_save();
        }

        switch (choice) {
            case '1':
                printf("-> Starting Clock\n");
                run_clock_app(vfd);
                break;

            case '2':
                printf("-> Starting Display Test Harness\n");
                run_display_test_app(vfd);
                break;

            case '3':
                printf("-> Starting Animation\n");
                run_animation_app(vfd, resume);
                break;

            case '4':
                printf("-> Starting Static Text\n");
                run_static_text_app(vfd, resume);
                break;

            case 'r':
            case 'R':
                if (confirm_factory_reset()) {
                    bool saved = settings_factory_reset();
                    // Reset live hardware as well as persisted preferences.
                    // Clock::init() normally preserves a running RTC.
                    Clock clock(vfd);
                    clock.reset();
                    vfd.reset();
                    printf(saved ? "-> Factory reset complete; defaults restored\n" :
                                   "-> Defaults are active; persistent reset incomplete. Retry R before rebooting\n");
                } else {
                    printf("\n-> Factory reset cancelled\n");
                }
                break;

            default:
                printf("Unrecognized selection: '%c'. Please choose 1, 2, 3, 4, or R.\n", choice);
                break;
        }

        // Start the next menu prompt from a clean display, whether an app
        // just exited or the selection was invalid.
        vfd.clear_screen();
        next_app = AppId::MENU;
        resume = false;
        // Keep the last selected app saved for the next reboot; this
        // session stays at the launcher until another selection is made.
    }
}
