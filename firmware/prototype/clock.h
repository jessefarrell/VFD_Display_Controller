#ifndef CLOCK_H
#define CLOCK_H

#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "hardware/rtc.h"
#include "config.h"
#include "vfd.h"
#include <stdint.h>

inline constexpr const char* days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
inline constexpr const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

enum class Display_Mode {
    DEFAULT, MODE1, MODE2, MODE3

    // DEFAULT
    // Expected clock format "MMMDD---DDD---HHMMXX"
    // Example "JUL05---SUN---0545PM"

    // MODE1 - Undefined
    // MODE2 - Undefined
    // MODE3 - Undefined
};

class Clock {

    private:
    Display_Mode mode;
    Vfd& vfd;

    /**
     * @brief Reads the current date/time from the RP2040 hardware RTC.
     *
     * @return datetime_t - current date/time as tracked by the RTC.
     */
    datetime_t _get_datetime(void);

    /**
     * @brief Writes a date/time back to the RP2040 hardware RTC.
     *
     * @param dt - date/time to set.
     */
    void _set_datetime(const datetime_t& dt);

    /**
     * @brief Computes day-of-week for a given date (Zeller's congruence),
     * since the RTC does not derive dotw automatically -- it must be
     * supplied whenever year/month/day change.
     *
     * @param dt - date to compute weekday for (year/month/day used, dotw ignored)
     * @return int8_t - 0=SUN ... 6=SAT
     */
    int8_t _compute_dotw(const datetime_t& dt);

    public:

    /**
     * @brief Constructs a Clock bound to the given Vfd display.
     *
     * @param vfd_ref - reference to an already-constructed Vfd instance.
     * Clock does not own or initialize the Vfd -- call vfd.init() separately.
     */
    Clock(Vfd& vfd_ref);

    /**
     * @brief Initializes the hardware RTC and starts displaying clock.
     *
     * By default the time is set to 00:00:00, JAN 1 2026 (Thursday).
     * Note: the RP2040 RTC is NOT battery-backed -- this default is
     * re-applied on every power-on/reset, not just the first boot.
     */
    void init(void);

    /**
     *  @brief Resets the RTC back to its default date/time.
     */
    void reset(void);

    /**
     * @brief Sets the minute field, leaving the rest of the date/time untouched.
     *
     * @param minute - desired minute (0-59)
     */
    void set_minute(int minute);

    /**
     * @brief Sets the hour field, leaving the rest of the date/time untouched.
     * Value set using 24hr format, so no need to specify AM/PM.
     *
     * @param hour - desired hour (0-23)
     */
    void set_hour(int hour);

    /**
     * @brief Sets the day-of-month field. Recomputes day-of-week to match.
     *
     * @param day - desired day of month (1-31)
     */
    void set_day(int day);

    /**
     * @brief Sets the month field. Recomputes day-of-week to match.
     *
     * @param month - desired month (1-12)
     */
    void set_month(int month);

    /**
     * @brief Sets the year field. Recomputes day-of-week to match.
     *
     * @param year - full 4-digit year
     */
    void set_year(int year);

    /**
     * @brief Formats the current date/time per `mode` and writes it to
     * the attached Vfd display.
     */
    void update_display(void);

};

/**
 * @brief Runs the clock application: initializes a Clock bound to the
 * given Vfd and refreshes the display once per second.
 *
 * Returns when the user presses Esc or Ctrl+C, so main.cpp can return to
 * its menu.
 *
 * @param vfd - reference to an already-initialized Vfd instance.
 */
void run_clock_app(Vfd& vfd);

#endif