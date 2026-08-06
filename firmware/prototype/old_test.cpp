/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "config.h"
#include "vfd.h"
#include <stdio.h>
// #include <stdlib.h>

#define WRITE_TIME_US 100

int pico_init(void) {
    // Set all pins as outputs
    gpio_init_mask(0xffffff);
    gpio_set_dir_out_masked(0xffffff);
    return 1;
}

int vfd_init(void){
    /**
     * @brief Sets default state for all the pins of the VFD
     */

    gpio_put(VFD_SERIAL, 0);
    gpio_put(VFD_nSEL, 1);   // Only one device on bus this can always stay low
    gpio_put(VFD_nRD, 0);
    gpio_put(VFD_A0, 1);    // To enter characters normally this needs to be pulled low
    gpio_put(VFD_nWR, 0);
    gpio_put(VFD_D0, 0);
    gpio_put(VFD_D1, 0);
    gpio_put(VFD_D2, 0);
    gpio_put(VFD_D3, 0);
    gpio_put(VFD_D4, 0);
    gpio_put(VFD_D5, 0);
    gpio_put(VFD_D6, 0);
    gpio_put(VFD_D7, 0);

    // Clear the display
    gpio_put(VFD_nRESET, 0);
    sleep_ms(10);
    gpio_put(VFD_nRESET, 1);
    sleep_ms(10);
    gpio_put(VFD_nRESET, 0);
    return 1;
}

void vfd_put_data(uint8_t code){
/**
 * @brief pushes data onto the 8 bit bus and clocks it out
 * 
 * Sets data on pins D0-D7 then clocks it out using nWR.
 * 
 * @param code 8 bit code defining desired values on D0-D7.
 */
    const uint32_t data_mask = 0xFFu << VFD_D0;      // covers GP7-GP14
    const uint32_t data_bits = ((uint32_t)code) << VFD_D0;

    // Place data on the bus
    gpio_put_masked(data_mask, data_bits);

    // Strobe -WR (active low): assert, hold, release
    // Reminder we are using pull-downs, so logic is inverted
    sleep_us(WRITE_TIME_US);  
    gpio_put(VFD_nWR, 1);
    sleep_us(WRITE_TIME_US);   
    gpio_put(VFD_nWR, 0);
    sleep_us(WRITE_TIME_US);
}

char read_char(void) {
    int c = getchar_timeout_us(UINT32_MAX);  // block indefinitely

    if (c == PICO_ERROR_TIMEOUT) {
        return '\0';  // shouldn't happen with UINT32_MAX, but just in case
    }

    putchar((char)c);  // echo the character back

    return (char)c;
}

int main() {
    stdio_init_all();
    pico_init();
    vfd_init();

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    uint8_t code = 0;
    uint8_t not_code = 0;

    char line[16];


    while(true){
        // gpio_put(VFD_SERIAL, 1);
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(50);
        // gpio_put(VFD_SERIAL, 0);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        sleep_ms(50);

        printf("Enter Code Code: \n");
        code = (int)read_char();
        
        // Invert it
        not_code = ~code & 0xff;

        printf("Test Code: 0x%02x | %d\n", code, code);
        vfd_put_data(not_code);
    }
}
