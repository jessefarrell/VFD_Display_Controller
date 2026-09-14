#ifndef CONFIG_H
#define CONFIG_H

/* =====================================================================
 * VFD Module Configuration
 * Target: Raspberry Pi Pico (RP2040)
 * Module: VFD module FU209SCPB-T60A harvested from ACM 3710 energy meter
 * Board: VfdController (Shamrock Circuits) - Pico signals are routed
 *        through a pair of 74LVC4245 (U2/U3) level translators before
 *        reaching the 6-pin power header and 26-pin data header.
 * =====================================================================
 *
 * ---------------------------------------------------------------------
 * 6-Pin Power Header (J4)
 * ---------------------------------------------------------------------
 *   Pin 1 -> +5V        (power supply, not a Pico GPIO)
 *   Pin 4 -> GND        (ground, not a Pico GPIO)
 *   Pin 6 -> -RESET     -> U2 (level translator) -> Pico GP13
 *
 * ---------------------------------------------------------------------
 * 26-Pin Data Header (J2)
 * ---------------------------------------------------------------------
 *   All even pins (2,4,6,...,26) -> GND
 *
 *   Pin 1  -> SERIAL    -> U3 -> Pico GP0
 *   Pin 3  -> SEL       -> U3 -> Pico GP1
 *   Pin 5  -> -RD       -> U3 -> Pico GP2
 *   Pin 7  -> A0        -> U3 -> Pico GP3
 *   Pin 9  -> -WR       -> U3 -> Pico GP4
 *   Pin 11 -> D0        -> U3 -> Pico GP5
 *   Pin 13 -> D1        -> U3 -> Pico GP6
 *   Pin 15 -> D2        -> U3 -> Pico GP7
 *   Pin 17 -> D3        -> U2 -> Pico GP8
 *   Pin 19 -> D4        -> U2 -> Pico GP9
 *   Pin 21 -> D5        -> U2 -> Pico GP10
 *   Pin 23 -> D6        -> U2 -> Pico GP11
 *   Pin 25 -> D7        -> U2 -> Pico GP12
 *
 * Note: signals prefixed with 'n' (e.g. VFD_nRESET, VFD_nRD, VFD_nWR)
 * are active-low, matching the "-" notation in the source pinout.
 * =====================================================================
 */

 #define SETUP_HOLD_TIME_US 50
 

/* ---- Control / Status Pins ---- */
#define VFD_nRESET   13   /* -RESET, from 6-pin power header (J4), pin 6 */
#define VFD_SERIAL   0    /* SERIAL, unused on ACM 3710, data header (J2) pin 1 */
#define VFD_nSEL      1    /* -SEL, data header (J2) pin 3 */
#define VFD_nRD      2    /* -RD, data header (J2) pin 5 */
#define VFD_A0       3    /* A0, data header (J2) pin 7 */
#define VFD_nWR      4    /* -WR, data header (J2) pin 9 */

/* ---- Data Bus Pins (D0-D7) ---- */
#define VFD_D0       5    /* data header (J2) pin 11 */
#define VFD_D1       6    /* data header (J2) pin 13 */
#define VFD_D2       7    /* data header (J2) pin 15 */
#define VFD_D3       8    /* data header (J2) pin 17 */
#define VFD_D4       9    /* data header (J2) pin 19 */
#define VFD_D5       10   /* data header (J2) pin 21 */
#define VFD_D6       11   /* data header (J2) pin 23 */
#define VFD_D7       12   /* data header (J2) pin 25 */

#endif /* CONFIG_H */