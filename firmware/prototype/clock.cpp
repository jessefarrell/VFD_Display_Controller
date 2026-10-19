#include "clock.h"
#include "app_common.h"
#include "settings.h"
#include "pico/time.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ---------------------------------------------------------------------------
// ASSUMPTION: MODE1/MODE2/MODE3 formats are undefined per clock.h -- until
// they're specified, update_display() falls back to the DEFAULT format for
// all modes. Revisit once MODE1-3 are designed.
// ---------------------------------------------------------------------------

namespace {
    constexpr uint64_t CHECKPOINT_INTERVAL_US = 60ull * 1000000;

    void remember_datetime(const datetime_t& dt) {
        auto& s = settings();
        s.year = dt.year;
        s.month = dt.month;
        s.day = dt.day;
        s.hour = dt.hour;
        s.minute = dt.min;
        s.second = dt.sec;
    }

    bool _is_leap_year(int year) {
        return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    }

    // Local-only day-in-month table, used purely to sanity-clamp setter
    // input before it reaches the RTC -- NOT re-exposed via clock.h, since
    // the RTC hardware itself is the source of truth for calendar rollover.
    int _days_in_month(int month, int year) {
        static const int table[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        int days = table[month - 1];
        if (month == 2 && _is_leap_year(year)) {
            days = 29;
        }
        return days;
    }

    // Distinguishes "user typed a value" from "user pressed Enter to keep
    // the default" from "user hit Esc/Ctrl+C" -- the set-datetime wizard
    // needs all three, since only the last one should cancel the rest of
    // the wizard rather than just moving on with the current value.
    enum class FieldInput { ENTERED, KEPT_DEFAULT, ABORTED };

    /**
     * @brief Blocking read of a short digits-only line from stdin,
     * terminated by Enter. Backspace/Delete removes the last digit.
     *
     * @param out_value - set to the parsed value if the result is ENTERED;
     * untouched otherwise.
     */
    FieldInput _read_uint_line(uint32_t* out_value) {
        char buf[8] = {0};
        size_t len = 0;

        while (true) {
            char c = read_char();

            if (is_exit_key(c)) {
                printf("\n");
                return FieldInput::ABORTED;
            }
            if (c == '\r' || c == '\n') {
                printf("\n");
                break;
            }
            if ((c == 0x7F || c == 0x08) && len > 0) {  // Delete / Backspace
                len--;
                buf[len] = '\0';
                printf(" \b");  // erase the character just echoed by read_char
                continue;
            }
            if (c >= '0' && c <= '9' && len < sizeof(buf) - 1) {
                buf[len++] = c;
                buf[len] = '\0';
            }
            // Any other key is silently ignored -- this field is digits only.
        }

        if (len == 0) {
            return FieldInput::KEPT_DEFAULT;
        }
        *out_value = (uint32_t)atoi(buf);
        return FieldInput::ENTERED;
    }

    /**
     * @brief Prompts for one numeric field, showing current_value as the
     * default. On KEPT_DEFAULT or ABORTED, *out_value is set to
     * current_value; on ENTERED, to whatever was typed.
     *
     * @return the same FieldInput result from _read_uint_line(), so the
     * caller can tell an abort apart from an accepted default.
     */
    FieldInput _prompt_field(const char* label, int current_value, int* out_value) {
        printf("%s (Enter to keep %d): ", label, current_value);

        uint32_t value;
        FieldInput result = _read_uint_line(&value);
        *out_value = (result == FieldInput::ENTERED) ? (int)value : current_value;
        return result;
    }

    /**
     * @brief Interactive wizard to set the clock's date/time, one field
     * at a time (year, then month, then day, then hour, then minute --
     * in that order so each field's clamping sees the already-updated
     * year/month, e.g. a day of 29 lands correctly whether or not the
     * new year makes February a leap month). Esc/Ctrl+C at any prompt
     * cancels the rest of the wizard; fields already confirmed stay
     * applied, since each is written to the RTC immediately.
     */
    bool _run_set_datetime_wizard(Clock& clock) {
        datetime_t dt = clock.get_datetime();

        printf("\n--- Set Date/Time --- (Esc/Ctrl+C cancels the rest)\n");

        int year, month, day, hour, minute;

        if (_prompt_field("Year", dt.year, &year) == FieldInput::ABORTED) {
            printf("-> Cancelled, no changes made\n");
            return false;
        }
        clock.set_year(year);

        if (_prompt_field("Month (1-12)", dt.month, &month) == FieldInput::ABORTED) {
            printf("-> Cancelled -- Year updated, rest unchanged\n");
            return true;
        }
        clock.set_month(month);

        if (_prompt_field("Day (1-31)", dt.day, &day) == FieldInput::ABORTED) {
            printf("-> Cancelled -- Year/Month updated, rest unchanged\n");
            return true;
        }
        clock.set_day(day);

        if (_prompt_field("Hour, 24hr (0-23)", dt.hour, &hour) == FieldInput::ABORTED) {
            printf("-> Cancelled -- Year/Month/Day updated, rest unchanged\n");
            return true;
        }
        clock.set_hour(hour);

        if (_prompt_field("Minute (0-59)", dt.min, &minute) == FieldInput::ABORTED) {
            printf("-> Cancelled -- everything but Minute updated\n");
            return true;
        }
        clock.set_minute(minute);

        printf("-> Date/time updated\n");
        return true;
    }
}

Clock::Clock(Vfd& vfd_ref) : mode(Display_Mode::DEFAULT), vfd(vfd_ref) {}

datetime_t Clock::_get_datetime(void) {
    datetime_t dt;
    rtc_get_datetime(&dt);
    return dt;
}

void Clock::_set_datetime(const datetime_t& dt) {
    datetime_t tmp = dt;
    if (rtc_set_datetime(&tmp)) {
        // The SDK requires up to three RTC clock cycles before reads see
        // the new value (64 us at the default Pico RTC clock frequency).
        sleep_us(64);
    } else {
        printf("-> RTC rejected date/time\n");
    }
}

int8_t Clock::_compute_dotw(const datetime_t& dt) {
    // Zeller's congruence (Gregorian calendar).
    int day = dt.day;
    int month = dt.month;
    int year = dt.year;

    if (month < 3) {
        month += 12;
        year -= 1;
    }

    int K = year % 100;
    int J = year / 100;

    // h: 0=Saturday, 1=Sunday, 2=Monday, ... 6=Friday
    int h = (day + (13 * (month + 1)) / 5 + K + K / 4 + J / 4 + 5 * J) % 7;

    // Convert to 0=Sunday ... 6=Saturday to match days[] and datetime_t.dotw
    return static_cast<int8_t>((h + 6) % 7);
}

void Clock::init(void) {
    mode = Display_Mode::DEFAULT;
    if (!rtc_running()) {
        rtc_init();
        const auto& s = settings();
        datetime_t dt = {};
        dt.year = s.year;
        dt.month = s.month;
        dt.day = s.day;
        dt.hour = s.hour;
        dt.min = s.minute;
        dt.sec = s.second;
        dt.dotw = _compute_dotw(dt);
        _set_datetime(dt);
        if (s.clock_time_set)
            printf("-> Restored saved date/time; elapsed power-off time is unknown\n");
    }
    last_checkpoint_us = time_us_64();
    update_display();
}

void Clock::reset(void) {
    if (!rtc_running()) rtc_init();
    datetime_t dt = {};
    dt.year  = 2026;
    dt.month = 1;
    dt.day   = 1;
    dt.hour  = 0;
    dt.min   = 0;
    dt.sec   = 0;
    dt.dotw  = _compute_dotw(dt);

    _set_datetime(dt);
    last_checkpoint_us = time_us_64();
    update_display();
}

datetime_t Clock::get_datetime(void) {
    return _get_datetime();
}

bool Clock::save_datetime(void) {
    datetime_t dt = _get_datetime();
    settings().clock_time_set = true;
    remember_datetime(dt);
    last_checkpoint_us = time_us_64();
    return settings_save(true);
}

void Clock::checkpoint_if_due(void) {
    uint64_t now = time_us_64();
    if (now - last_checkpoint_us < CHECKPOINT_INTERVAL_US) return;
    // Rate-limit failures too; retrying every display refresh would waste flash.
    last_checkpoint_us = now;
    if (!settings().clock_time_set) return;
    datetime_t dt{};
    if (!rtc_get_datetime(&dt)) return;
    remember_datetime(dt);
    settings_checkpoint_clock();
}

void Clock::set_minute(int minute) {
    if (minute < 0)  minute = 0;
    if (minute > 59) minute = 59;

    datetime_t dt = _get_datetime();
    dt.min = minute;
    _set_datetime(dt);
    update_display();
}

void Clock::set_hour(int hour) {
    if (hour < 0)  hour = 0;
    if (hour > 23) hour = 23;

    datetime_t dt = _get_datetime();
    dt.hour = hour;
    _set_datetime(dt);
    update_display();
}

void Clock::set_day(int day) {
    datetime_t dt = _get_datetime();

    int max_day = _days_in_month(dt.month, dt.year);
    if (day < 1)        day = 1;
    if (day > max_day)  day = max_day;

    dt.day = day;
    dt.dotw = _compute_dotw(dt);
    _set_datetime(dt);
    update_display();
}

void Clock::set_month(int month) {
    datetime_t dt = _get_datetime();

    if (month < 1)  month = 1;
    if (month > 12) month = 12;
    dt.month = month;

    // Clamp day in case the new month has fewer days than the current one
    // (e.g. Jan 31 -> Feb -> Feb 28/29)
    int max_day = _days_in_month(dt.month, dt.year);
    if (dt.day > max_day) dt.day = max_day;

    dt.dotw = _compute_dotw(dt);
    _set_datetime(dt);
    update_display();
}

void Clock::set_year(int year) {
    datetime_t dt = _get_datetime();
    if (year < 1) year = 1;
    if (year > 4095) year = 4095;
    dt.year = year;

    // Re-clamp day in case a leap-day (Feb 29) no longer exists in the new year
    int max_day = _days_in_month(dt.month, dt.year);
    if (dt.day > max_day) dt.day = max_day;

    dt.dotw = _compute_dotw(dt);
    _set_datetime(dt);
    update_display();
}

void Clock::update_display(void) {
    datetime_t dt = _get_datetime();

    int display_hour = dt.hour % 12;
    if (display_hour == 0) display_hour = 12;
    const char* am_pm = (dt.hour < 12) ? "AM" : "PM";

    char buf[Vfd::MAX_DIGITS+1];

    switch (mode) {
        case Display_Mode::MODE1:
        case Display_Mode::MODE2:
        case Display_Mode::MODE3:
            // ASSUMPTION: falls through to DEFAULT until these are specified.
        case Display_Mode::DEFAULT:
        default:
            std::snprintf(buf, sizeof(buf), "%s%02d---%s---%02d%02d%s",
                          months[dt.month - 1],
                          dt.day,
                          days[dt.dotw],
                          display_hour,
                          dt.min,
                          am_pm);
            break;
    }

    // clear_screen() intentionally does not move the cursor (see its doc
    // comment in vfd.h) -- if a previous app left it partway across the
    // display, write_string() below would start mid-line and wrap the
    // tail end around onto the front, scrambling the output. Explicitly
    // home it first, same as every other app's full-line writes do.
    vfd.clear_screen();
    sleep_ms(1);
    vfd.cursor_move(0);
    vfd.write_string(buf);
    checkpoint_if_due();
}

void run_clock_app(Vfd& vfd) {
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    Clock clock(vfd);
    clock.init();

    printf("\n--- Clock ---\n");
    printf("Press 's' to set the date/time. Esc / Ctrl+C exits to the main menu.\n");
    printf("Once set, date/time is checkpointed about once a minute while Clock is running.\n");

    while (true) {
        // Heartbeat LED so you can confirm the board is alive.
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(1);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        sleep_ms(1);

        int key = poll_char();
        if (key != -1) {
            char c = (char)key;
            if (is_exit_key(c)) {
                printf("-> Exiting Clock, returning to menu\n");
                return;
            } else if (c == 's' || c == 'S') {
                if (_run_set_datetime_wizard(clock)) clock.save_datetime();
            }
            // Any other key is ignored.
        }

        clock.update_display();

        // Wake for any key and let the input loop handle it, including
        // 's' typed during this wait. The wait does not consume the key.
        wait_for_input(1000);
    }
}
