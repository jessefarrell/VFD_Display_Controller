/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "test_display.h"
#include "app_common.h"
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "config.h"
#include "vfd.h"
#include <stdio.h>

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

static void print_menu(void) {
    printf("\n--- VFD Class Test Harness ---\n");
    printf("Type printable characters to write them to the display.\n");
    printf("Control keys:\n");
    printf("  [Backspace]   cursor_shift(LEFT, 1)\n");
    printf("  [Tab]         cursor_shift(RIGHT, 1)\n");
    printf("  [Enter]       cursor_move(0)          (home)\n");
    printf("  [Delete/0x7F] delete_character(1)\n");
    printf("  Ctrl+B (0x02) toggle cursor_blink()\n");
    printf("  Ctrl+L (0x0C) clear_screen()\n");
    printf("  Ctrl+R (0x12) reset()\n");
    printf("  Ctrl+\\ (0x1C) display_scroll(LEFT, wrap=true)\n");
    printf("  Ctrl+] (0x1D) display_scroll(RIGHT, wrap=true)\n");
    printf("  Ctrl+H (0x08) same as Backspace (cursor left)\n");
    printf("  Ctrl+A (0x01) print this menu again\n");
    printf("  Esc / Ctrl+C  exit to menu\n");
    printf("-------------------------------\n\n");
}

void run_display_test_app(Vfd& vfd) {
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    print_menu();

    static bool blink_state = false;

    while (true) {
        // Heartbeat LED so you can confirm the board is alive and not stuck
        // waiting on something unexpected.
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(1);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        sleep_ms(1);

        printf("Enter input: ");
        char c = read_char();
        printf("\n");

        switch (c) {
            case KEY_ESC:     // also Ctrl+[ -- reserved for exit, see below
            case KEY_CTRL_C:
                printf("-> Exiting Display Test Harness, returning to menu\n");
                return;

            case 0x7F:  // Delete
                printf("-> delete_character(1)\n");
                vfd.delete_character(1);
                break;

            case '\t':  // Tab
                printf("-> cursor_shift(RIGHT, 1)\n");
                vfd.cursor_shift(Direction::RIGHT, 1);
                break;

            case 0x08:  // Backspace / Ctrl+H
                printf("-> cursor_shift(LEFT, 1)\n");
                vfd.cursor_shift(Direction::LEFT, 1);
                break;

            case '\r':  // Enter
            case '\n':
                printf("-> cursor_move(0)\n");
                vfd.cursor_move(0);
                break;

            case 0x02:  // Ctrl+B
                blink_state = !blink_state;
                printf("-> cursor_blink(%s)\n", blink_state ? "true" : "false");
                vfd.cursor_blink(blink_state);
                break;

            case 0x0C:  // Ctrl+L
                printf("-> clear_screen()\n");
                vfd.clear_screen();
                break;

            case 0x12:  // Ctrl+R
                printf("-> reset()\n");
                vfd.reset();
                break;

            case 0x1C:  // Ctrl+\ (moved off Ctrl+[/Esc, which now exits)
                printf("-> display_scroll(LEFT, wrap=true)\n");
                vfd.display_scroll(Direction::LEFT, true);
                break;

            case 0x1D:  // Ctrl+]
                printf("-> display_scroll(RIGHT, wrap=true)\n");
                vfd.display_scroll(Direction::RIGHT, true);
                break;

            case 0x01:  // Ctrl+A
                print_menu();
                break;

            default:
                if (c >= 0x20 && c < 0x7F) {  // printable ASCII
                    printf("-> write_char('%c')\n", c);
                    bool ok = vfd.write_char(c);
                    if (!ok) {
                        printf("   write_char returned false (cursor at end?)\n");
                    }
                } else {
                    printf("-> unrecognized input: 0x%02x (ignored)\n", (uint8_t)c);
                }
                break;
        }
    }
}
