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
#include <stdio.h>

// Single global instance, shared by whichever app the user selects below.
Vfd vfd(SETUP_HOLD_TIME_US);

/**
 * @brief Blocking read of one character from stdin, echoed back to console.
 */
static char read_char(void) {
    int c = getchar_timeout_us(UINT32_MAX);  // block indefinitely

    if (c == PICO_ERROR_TIMEOUT) {
        return '\0';  // shouldn't happen with UINT32_MAX, but just in case
    }

    putchar((char)c);
    return (char)c;
}

static void print_app_menu(void) {
    printf("\n--- VFD Launcher ---\n");
    printf("  [1] Clock\n");
    printf("  [2] Display Test Harness\n");
    printf("  [3] Animation\n");
    printf("Select an application: ");
}

int main() {
    stdio_init_all();
    sleep_ms(2000);  // give USB serial time to enumerate before first prints

    vfd.init();

    // Redisplay the menu every time an app exits (Esc/Ctrl+C) or an
    // invalid key was pressed.
    while (true) {
        print_app_menu();
        char choice = read_char();
        printf("\n");

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
                run_animation_app(vfd);
                break;

            default:
                printf("Unrecognized selection: '%c'. Please choose 1, 2, or 3.\n", choice);
                break;
        }

        // Start the next menu prompt from a clean display, whether an app
        // just exited or the selection was invalid.
        vfd.clear_screen();
    }
}
