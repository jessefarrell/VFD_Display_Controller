
#ifndef CLOCK_H
#define CLOCK_H

#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "config.h"
#include <stdint.h>

const char *days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const char *months[]= {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
const uint8_t days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 30};

enum class Display_Mode{
    DEFAULT, MODE1, MODE2, MODE3

    // DEFAULT
    // Expected clock format "MMMDD---DDD---HHMMXX"
    // Example "JUL05---SUN--0545PM"
};

class Clock{

    private:
    Display_Mode mode;
    uint8_t day_index;    
    uint8_t month_index;
    uint8_t year;
    uint8_t date;
    uint8_t hour;
    uint8_t minute;


    /**
     * @brief Checks if the year is a leap year
     * 
     * @return bool - True if current year is a leap year (29 days in Feb).
     */
    bool _leap_year_check(uint8_t current_year);

    public:

    /**
     * @brief Sets up any HW registers than calls init
     */
    Clock(void);

    /**
     * @brief Initializes private variables, and starts displaying clock.
     * 
     * By default the time will be set to 12:00.
     */
    void init(void);

    /**
     * @brief  Increments the minute counter by 1
     */
    void increment_mins(void);

    /**
     * @brief Sets the minute variable to the specified value
     * 
     * @param uint8_t minute - desired minute to set
     */
    void set_minute(uint8_t minute);

    /**
     * @brief Sets the hour variable to the specified value
     * 
     * @param uint8_t hour - desired hour to set
     */
    void set_hour(uint8_t hour);

    /**
     * @brief Set AM/PM state
     */



    

};

#endif