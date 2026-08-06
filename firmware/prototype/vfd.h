
// #include <stdint.h>
#ifndef VFD_H
#define VFD_H

// #include <stdint.h>
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "config.h"
#include <stdint.h>

enum class VfdCode : uint8_t{
    CURSOR_ENABLE = 15,
    CURSOR_DISABLE = 14,
    CURSOR_MOVE_RIGHT = 9,
    CURSOR_MOVE_LEFT = 8,
    CURSOR_MOVE_HOME = 13,
    DELETE_ELEMENT = 32, // Sends ASCII space to blank the digit at cursor position
    CLEAR_SCREEN = 10,
    SPACE = 13

    // NOTE most ascii codes are unchanges
    // EX: A-Z, 0-9, +,-,/... ect
};

enum class Direction { RIGHT = 1, LEFT = 0 };

class Vfd{
private:
    static constexpr uint8_t MAX_DIGITS = 20;
    uint8_t setup_hold_time_us;
    uint8_t cursor_position;                // 0 = Furthest left (home), MAX_DIGITS -1 = furthest right
    char displayed_string[MAX_DIGITS];

    /**
     * @brief Writes 8 bit code to D0-D7
     * 
     * Function assumes bus is already setup.
     * Data is clocked in as defined by setup_hold_time_us.
     * To do this the function must
     *  1) Assert SEL
     *  2) Set data pins
     *  3) Pulse -WR pin
     *  4) De-assert SEL
     *  5) De-assert data
     * 
     *  Special Characters
     *  > The displayed character for "S" looks bad use "5" instead, replace 0x53 with 0x35
     * 
     * @param data - 8 bit value representing [D7, D6, D5... D0] to be written.
     */
    void _write_code(uint8_t data);

    /**
     * @brief Sets cursor to its home position
     */
    void _cursor_send_home(void);

    /**
     * @brief - Clears the display and then writes display_string to it.
     * 
     * This function can be used by various other public methods
     * The function clears the display and then writes the contexts of displayed_string to it
     */
    void _clear_and_update_display(void);

    /**
     * @brief Writes a logical 8-bit value to the D0-D7 data bus, accounting
     * for the external pull-down inversion on the bus.
     *
     * The physical bus is inverted by external hardware (pull-downs), so a
     * logical 1 on a given D-line requires driving the corresponding Pico
     * pin LOW, and vice versa. This function is the single place that
     * inversion is applied -- callers always think in terms of the logical
     * (non-inverted) byte they want the VFD to see.
     *
     * @param data - 8 bit logical value representing [D7, D6, D5... D0].
     */
    void _set_data_bus(uint8_t data);

    /**
     * @brief Wrapper around gpio_put() for a single data-bus pin that
     * centralizes the inverted-logic hardware quirk.
     *
     * Hardware note: external pull-downs on the D0-D7 lines mean a logical
     * HIGH must be driven electrically LOW at the Pico pin, and vice versa
     * (confirmed empirically via `not_code = ~code & 0xff` in the original
     * proof-of-concept). Callers of this function think only in terms of
     * the logical bit value they want on the bus; the physical inversion
     * happens here and nowhere else.
     *
     * @param pin - GPIO pin number for a single D0-D7 line
     * @param value - desired LOGICAL bit value (true = logical 1)
     */
    void _vfd_gpio_put(uint pin, bool value);

    /**
     * @brief Resets displayed_string to all spaces with a single null
     * terminator at index MAX_DIGITS.
     *
     * Used instead of a raw memset(..., 0, ...) so "empty" digit positions
     * are represented by ' ' rather than '\0'. A null byte anywhere before
     * the end of the buffer breaks every C-string-style consumer of
     * displayed_string (write_string's loop, any %s printf) by terminating
     * early, even when real characters exist further along.
     */
    void _blank_displayed_string(void);
public:

    /**
     * @brief Constructor for VFD class
     * 
     * Setup and hold times defined by (t). To initialize you must still call Vfd.init().
     * 
     * @param t - Setup and hold time. Same value assumed for both.
     */
    Vfd(uint8_t t);

    /**
     * @brief Initializes and resets the system
     * 
     * Sets all the pins to a known state, and issues a reset to clear the screen.
     */
    void init(void);

    /**
     * @brief Reset the VFD module.
     * 
     * A module reset, resets the module back to its factory default
     * Clears the screen, disables the cursor, and moves the cursor to home
     */
    void reset(void);

    /**
     * @brief Clears all characters from the screen
     * 
     * This command will clear the screen but does not reset the module.
     * Cursor position will not change
     */
    void clear_screen(void);

    /**
     * @brief Writes the desired character to the terminal
     * 
     * Character will be written to current cursor position.
     * 
     * @param c - character to be written.
     * @return bool - false when text wraps to a new line.
     */
    bool write_char(char c);

    /**
     * @brief Writes the string of text to the screen
     * 
     * Uses write_char to write each character in text to the screen.
     * Number of characters printed will be limited by MAX_DIGITS
     * 
     * @return bool - false if called function write_char returns false for any character
     */
    bool write_string(const char* text); 

    /**
     * @brief Enables and disables the cursor blinking
     * 
     * @param bool state - true to show cursor, false to hide cursor
     */
    void cursor_blink(bool state);

    /**
     * @brief Shifts cursor to the right or left
     * 
     * @param bool direction - true to shift right, false to shift left
     * @param uint8_t num - number of digits to shift by
     */
    void cursor_shift(Direction direction, uint8_t num);

    /**
     * @brief Deletes the current element on the screen.
     * 
     * Cursor increments to the right after it deletes character.
     * Function adjusts cursor_position accordingly
     * 
     * @param uint8_t num - number of elements to delete
     */
    void delete_character(uint8_t num);

    /**
     * @brief Jumps the cursor to the designated position
     * 
     * NOTE - If cursor is outside valid range it will be updated to either
     * the furthest left or furthest right element depending on the input.
     * Uses function cursor_shift
     * 
     * @param uint8_t position - desired cursor position.
     * @return bool - returns false if position is outside valid range.
     */
    bool cursor_move(uint8_t position);

    /**
     * @brief shifts all characters in the displayed text by 1
     * 
     * NOTE - Function uses the _clear_and_update_display function
     * 
     * @param direction - true shift right, false shift left
     * @param wrap - true last element wraps around to element 0
     */
    void display_scroll(Direction direction, bool wrap);
};

#endif