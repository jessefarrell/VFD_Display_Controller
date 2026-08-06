#include "vfd.h"
#include <string.h>
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "config.h"
#include "vfd.h"
#include <stdio.h>


// ---------------------------------------------------------------------------
// NOTE: The following behaviors were reverse-engineered from test.cpp and are
// NOT fully specified by vfd.h's doc comments. Flagged inline with "ASSUMPTION"
// or "DISCREPANCY" so they're easy to grep for and revisit.
// ---------------------------------------------------------------------------

Vfd::Vfd(uint8_t t) : setup_hold_time_us(t), cursor_position(0) {
    _blank_displayed_string();
}

void Vfd::_blank_displayed_string(void) {
    // Fill every digit position with a space, NOT a null byte. displayed_string
    // is treated as a null-terminated C-string all over this class (write_string's
    // text[i] != '\0' loop, every %s debug printf) -- if any "empty" position in
    // the middle of the buffer holds '\0' instead of ' ', those functions stop
    // dead right there, even though real characters exist further along. This
    // was the root cause of display_scroll appearing to produce an empty string:
    // the wrap-around "carry" character came from an unwritten, still-zeroed
    // position, planting a premature null terminator at index 0.
    memset(displayed_string, ' ', MAX_DIGITS);
    displayed_string[MAX_DIGITS] = '\0';
}

void Vfd::init(void) {
    // ASSUMPTION: mirrors pico_init() in test.cpp -- all VFD control/data
    // pins fall within GP0-GP23. If config.h ever moves a pin outside that
    // range this mask needs to change (or be built from the individual
    // VFD_* pin macros instead of a hardcoded literal).
    gpio_init_mask(0xFFFFFF);
    gpio_set_dir_out_masked(0xFFFFFF);

    // Mirrors vfd_init() default pin state from test.cpp.
    // CONFIRMED: every GPIO drives an NPN BJT base that pulls down the
    // corresponding VFD input pin, so ALL pins are inverted by design (not
    // just the data bus). The raw values below match test.cpp's gpio_put()
    // calls, which were hand-compensated for that inversion -- i.e. those
    // are the PHYSICAL GPIO states, not what the VFD actually sees.
    // _vfd_gpio_put()'s parameter is the desired LOGICAL/VFD-side state, so
    // each value here is the boolean opposite of test.cpp's raw int, which
    // the wrapper then flips back to reproduce the same physical GPIO state.
    _vfd_gpio_put(VFD_SERIAL, 1);
    _vfd_gpio_put(VFD_nSEL, 0);
    _vfd_gpio_put(VFD_nRD, 1);
    _vfd_gpio_put(VFD_A0, 0);
    _vfd_gpio_put(VFD_nWR, 1);
    _vfd_gpio_put(VFD_D0, 1);
    _vfd_gpio_put(VFD_D1, 1);
    _vfd_gpio_put(VFD_D2, 1);
    _vfd_gpio_put(VFD_D3, 1);
    _vfd_gpio_put(VFD_D4, 1);
    _vfd_gpio_put(VFD_D5, 1);
    _vfd_gpio_put(VFD_D6, 1);
    _vfd_gpio_put(VFD_D7, 1);

    reset();
}

void Vfd::reset(void) {
    _vfd_gpio_put(VFD_nRESET, 1);   // test.cpp: gpio_put(VFD_nRESET, 0)
    sleep_us(setup_hold_time_us);
    _vfd_gpio_put(VFD_nRESET, 0);  // test.cpp: gpio_put(VFD_nRESET, 1)
    sleep_us(setup_hold_time_us);
    _vfd_gpio_put(VFD_nRESET, 1);   // test.cpp: gpio_put(VFD_nRESET, 0)

    cursor_position = 0;
    _blank_displayed_string();
}

void Vfd::clear_screen(void) {
    _write_code(static_cast<uint8_t>(VfdCode::CLEAR_SCREEN));
    _blank_displayed_string();
}

void Vfd::_vfd_gpio_put(uint pin, bool value) {
    // GPIO drives an NPN BJT base which pulls down the corresponding VFD
    // input pin... `value` is the LOGICAL state you want the VFD to see.
    gpio_put(pin, !value);
}

void Vfd::_write_code(uint8_t data) {
    // Character substitution per vfd.h doc: "S" (0x53) displays poorly,
    // send "5" (0x35) instead.

    // Debug code print
    printf("_write_code: 0x%02x\r\n", data);

    if (data == 0x53 || data == 0x73) {
            data = 0x35;
        }

    static const uint data_pins[8] = {
        VFD_D0, VFD_D1, VFD_D2, VFD_D3, VFD_D4, VFD_D5, VFD_D6, VFD_D7
    };

    // Setup data pins
    // _vfd_gpio_put(VFD_nSEL, 0);
    for (int i = 0; i < 8; i++) {
        bool bit_value = (data >> i) & 0x01;
        _vfd_gpio_put(data_pins[i], bit_value);
    }

    // Send Write Pulse
    sleep_us(setup_hold_time_us);
    _vfd_gpio_put(VFD_nWR, 0);  // test.cpp: gpio_put(VFD_nWR, 1)
    sleep_us(setup_hold_time_us);
    _vfd_gpio_put(VFD_nWR, 1);   // test.cpp: gpio_put(VFD_nWR, 0)
    sleep_us(setup_hold_time_us);

    // Deselect the VFD
    // _vfd_gpio_put(VFD_nSEL, 1);
}

void Vfd::_cursor_send_home(void) {
    _write_code(static_cast<uint8_t>(VfdCode::CURSOR_MOVE_HOME));
    cursor_position = 0;
}

void Vfd::_clear_and_update_display(void) {
    // Snapshot BEFORE clear_screen(), because clear_screen() memsets
    // displayed_string to zero as a side effect. Without this snapshot,
    // write_string(displayed_string) below writes back an empty string --
    // this was the actual cause of display_scroll() appearing broken.
    char snapshot[MAX_DIGITS + 1];
    memcpy(snapshot, displayed_string, sizeof(snapshot));

    printf("_clear_and_update_display: snapshot = \"%s\"\r\n", snapshot);

    clear_screen();
    _cursor_send_home();
    write_string(snapshot);

    printf("_clear_and_update_display: displayed_string after write_string = \"%s\"\r\n", displayed_string);
}

bool Vfd::write_char(char c) {
    printf("\r\nCursor Position: %i", cursor_position);
    if (c < 0x20 || c > 0x7E) {
        return false;
    }

    _write_code(static_cast<uint8_t>(c));
    displayed_string[cursor_position] = c;

    // Valid cursor positions wrap from 0-(MAX_DIGITS-1) than back to zero
    if (cursor_position >= MAX_DIGITS-1) {
        _cursor_send_home();
        cursor_position=0;
    }else{
        cursor_position++;
    }

    return true;
}

bool Vfd::write_string(const char* text) {
    bool all_ok = true;

    for (size_t i = 0; text[i] != '\0' && cursor_position < MAX_DIGITS; i++) {
        if (!write_char(text[i])) {
            all_ok = false;
        }
    }
    return all_ok;
}

void Vfd::cursor_blink(bool state) {
    _write_code(static_cast<uint8_t>(
        state ? VfdCode::CURSOR_ENABLE : VfdCode::CURSOR_DISABLE));
}

void Vfd::cursor_shift(Direction direction, uint8_t num) {
    VfdCode code = (direction == Direction::RIGHT)
                       ? VfdCode::CURSOR_MOVE_RIGHT
                       : VfdCode::CURSOR_MOVE_LEFT;

    for (uint8_t i = 0; i < num; i++) {
        // Clamp so cursor_position can't run off either end of the buffer.
        if (direction == Direction::RIGHT) {
            if (cursor_position >= MAX_DIGITS) {
                break;
            }
            cursor_position++;
        } else {
            if (cursor_position == 0) {
                break;
            }
            cursor_position--;
        }
        _write_code(static_cast<uint8_t>(code));
    }
}

void Vfd::delete_character(uint8_t num) {
    for (uint8_t i = 0; i < num && cursor_position < MAX_DIGITS; i++) {
        _write_code(static_cast<uint8_t>(VfdCode::DELETE_ELEMENT));
        displayed_string[cursor_position] = ' ';    //Empty character is represented as a space ' '
        cursor_position++;
    }
}

bool Vfd::cursor_move(uint8_t position) {
    bool in_range = true;

    if (position >= MAX_DIGITS) {
        position = MAX_DIGITS - 1;
        in_range = false;
    }

    if (position > cursor_position) {
        cursor_shift(Direction::RIGHT, position - cursor_position);
    } else if (position < cursor_position) {
        cursor_shift(Direction::LEFT, cursor_position - position);
    }

    return in_range;
}

void Vfd::display_scroll(Direction direction, bool wrap) {
    char shifted[MAX_DIGITS + 1];
    memset(shifted, ' ', MAX_DIGITS);
    shifted[MAX_DIGITS] = '\0';

    printf("display_scroll: before = \"%s\"\r\n", displayed_string);

    if (direction == Direction::RIGHT) {
        // Grab carried characteR
        char carry = wrap ? displayed_string[MAX_DIGITS - 1] : ' ';
        shifted[0] = carry;

        // Shift text
        for (uint8_t i = 1; i < MAX_DIGITS; i++) {
            shifted[i] = displayed_string[i - 1];
        }
    } else {
        char carry = wrap ? displayed_string[0] : ' ';
        for (uint8_t i = 0; i < MAX_DIGITS - 1; i++) {
            shifted[i] = displayed_string[i + 1];
        }
        shifted[MAX_DIGITS - 1] = carry;
    }

    printf("display_scroll: shifted = \"%s\"\r\n", shifted);

    memcpy(displayed_string, shifted, sizeof(displayed_string));
    _clear_and_update_display();
}