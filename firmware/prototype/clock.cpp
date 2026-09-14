#include "clock.h"
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// ASSUMPTION: MODE1/MODE2/MODE3 formats are undefined per clock.h -- until
// they're specified, update_display() falls back to the DEFAULT format for
// all modes. Revisit once MODE1-3 are designed.
// ---------------------------------------------------------------------------

namespace {
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
}

Clock::Clock(Vfd& vfd_ref) : mode(Display_Mode::DEFAULT), vfd(vfd_ref) {}

datetime_t Clock::_get_datetime(void) {
    datetime_t dt;
    rtc_get_datetime(&dt);
    return dt;
}

void Clock::_set_datetime(const datetime_t& dt) {
    datetime_t tmp = dt;
    // NOTE: rtc_set_datetime() returns bool (success/failure) per the Pico
    // SDK -- currently ignored here. If hardware-level range rejection
    // matters to you (vs. the clamping done in the setters below), this
    // return value should be propagated up.
    rtc_set_datetime(&tmp);
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
    rtc_init();
    mode = Display_Mode::DEFAULT;
    reset();
}

void Clock::reset(void) {
    datetime_t dt = {};
    dt.year  = 2026;
    dt.month = 1;
    dt.day   = 1;
    dt.hour  = 0;
    dt.min   = 0;
    dt.sec   = 0;
    dt.dotw  = _compute_dotw(dt);

    _set_datetime(dt);
    update_display();
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

    char buf[MAX_DIGITS + 1];

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

    vfd.clear_screen();
    vfd.write_string(buf);
}