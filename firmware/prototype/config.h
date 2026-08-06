#ifndef CONFIG_H
#define CONFIG_H

/* =====================================================================
 * VFD Module Configuration
 * Target: Raspberry Pi Pico (RP2040)
 * Module: VFD module FU209SCPB-T60A harvested from ACM 3710 energy meter
 * =====================================================================
 *
 * ---------------------------------------------------------------------
 * 6-Pin Power Header
 * ---------------------------------------------------------------------
 *   Pin 1 -> +5V        (power supply, not a Pico GPIO)
 *   Pin 4 -> GND        (ground, not a Pico GPIO)
 *   Pin 6 -> -RESET     -> Pico GP17
 *
 * ---------------------------------------------------------------------
 * 26-Pin Data Header
 * ---------------------------------------------------------------------
 *   All even pins (2,4,6,...,26) -> GND
 *
 *   Pin 1  -> SERIAL    -> Pico GP2
 *   Pin 3  -> SEL       -> Pico GP3
 *   Pin 5  -> -RD       -> Pico GP4
 *   Pin 7  -> A0        -> Pico GP5
 *   Pin 9  -> -WR       -> Pico GP6
 *   Pin 11 -> D0        -> Pico GP7
 *   Pin 13 -> D1        -> Pico GP8
 *   Pin 15 -> D2        -> Pico GP9
 *   Pin 17 -> D3        -> Pico GP10
 *   Pin 19 -> D4        -> Pico GP11
 *   Pin 21 -> D5        -> Pico GP12
 *   Pin 23 -> D6        -> Pico GP13
 *   Pin 25 -> D7        -> Pico GP14
 *
 * Note: signals prefixed with 'n' (e.g. VFD_nRESET, VFD_nRD, VFD_nWR)
 * are active-low, matching the "-" notation in the source pinout.
 * =====================================================================
 */

/* ---- Control / Status Pins ---- */
#define VFD_nRESET   17   /* -RESET, from 6-pin power header, pin 6 */
#define VFD_SERIAL   2    /* SERIAL, unused on ACM 3710, data header pin 1 */
#define VFD_nSEL      3    /* -SEL, data header pin 3 */
#define VFD_nRD      4    /* -RD, data header pin 5 */
#define VFD_A0       5    /* A0, data header pin 7 */
#define VFD_nWR      6    /* -WR, data header pin 9 */

/* ---- Data Bus Pins (D0-D7) ---- */
#define VFD_D0       7    /* data header pin 11 */
#define VFD_D1       8    /* data header pin 13 */
#define VFD_D2       9    /* data header pin 15 */
#define VFD_D3       10   /* data header pin 17 */
#define VFD_D4       11   /* data header pin 19 */
#define VFD_D5       12   /* data header pin 21 */
#define VFD_D6       13   /* data header pin 23 */
#define VFD_D7       14   /* data header pin 25 */

#endif /* CONFIG_H */