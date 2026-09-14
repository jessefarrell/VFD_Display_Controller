#include "animation.h"
#include "app_common.h"
#include "pico/stdlib.h"
#include <cstdio>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Each _effect() function below is self-contained: it clears the screen,
// runs for a fixed number of frames, and returns. run_animation_app() picks
// one at random each time through its loop, so effects don't need to know
// about each other or share any state.
//
// All effects are built purely out of Vfd's public interface (write_char,
// cursor_move, display_scroll, clear_screen) -- no hardware/pin knowledge
// needed here.
//
// Each effect returns true if the user requested an exit (Esc/Ctrl+C)
// mid-effect, via sleep_or_exit(), so run_animation_app() can stop
// immediately instead of waiting for the whole effect to finish.
// ---------------------------------------------------------------------------

namespace {

/**
 * @brief Knight-Rider style scanner: a single character bounces back and
 * forth across the full width of the display.
 */
bool scanner_effect(Vfd& vfd) {
    vfd.clear_screen();
    const uint8_t last = Vfd::MAX_DIGITS - 1;

    for (int pass = 0; pass < 3; pass++) {
        for (uint8_t pos = 0; pos <= last; pos++) {
            vfd.cursor_move(pos);
            vfd.write_char('#');
            if (sleep_or_exit(60)) return true;
            vfd.cursor_move(pos);
            vfd.write_char(' ');
        }
        for (int pos = last; pos >= 0; pos--) {
            vfd.cursor_move((uint8_t)pos);
            vfd.write_char('#');
            if (sleep_or_exit(60)) return true;
            vfd.cursor_move((uint8_t)pos);
            vfd.write_char(' ');
        }
    }
    return false;
}

/**
 * @brief Scrolling marquee: a short message continuously wraps around the
 * display via display_scroll's wrap-around carry.
 */
bool marquee_effect(Vfd& vfd) {
    vfd.clear_screen();

    // Left-justify into a buffer exactly as wide as the display, padded
    // with spaces, so the wrap-around loop reads cleanly.
    char msg[Vfd::MAX_DIGITS + 1];
    std::snprintf(msg, sizeof(msg), "%-*s", (int)Vfd::MAX_DIGITS, " Schneider Electric ");

    vfd.cursor_move(0);
    vfd.write_string(msg);

    for (int i = 0; i < 40; i++) {
        if (sleep_or_exit(150)) return true;
        vfd.display_scroll(Direction::LEFT, true);
    }
    return false;
}

/**
 * @brief Random twinkling characters, like a field of stars.
 */
bool sparkle_effect(Vfd& vfd) {
    vfd.clear_screen();
    static const char sparkle_chars[] = {'*', '.', '+', 'o', '\''};
    const int num_chars = sizeof(sparkle_chars) / sizeof(sparkle_chars[0]);

    for (int frame = 0; frame < 60; frame++) {
        uint8_t pos = rand() % Vfd::MAX_DIGITS;
        char c = sparkle_chars[rand() % num_chars];
        vfd.cursor_move(pos);
        vfd.write_char(c);
        if (sleep_or_exit(80)) return true;

        // Periodically fade a random cell back to blank so the screen
        // doesn't just fill up solid.
        if (frame % 5 == 4) {
            uint8_t fade_pos = rand() % Vfd::MAX_DIGITS;
            vfd.cursor_move(fade_pos);
            vfd.write_char(' ');
        }
    }
    return false;
}

/**
 * @brief A rolling wave that travels across the display, built from a
 * small fixed "amplitude" shape rather than floating-point trig -- keeps
 * this effect dependency-free.
 */
bool wave_effect(Vfd& vfd) {
    vfd.clear_screen();

    static const char levels[] = {'.', '-', '=', '#', '=', '-'};
    const int num_levels = sizeof(levels) / sizeof(levels[0]);

    // One full up-and-down cycle of the wave, expressed as indices into
    // `levels`. Repeating/offsetting this against each column position
    // is what makes the wave appear to travel.
    static const int shape[] = {0, 1, 2, 3, 4, 5, 5, 4, 3, 2, 1, 0};
    const int shape_len = sizeof(shape) / sizeof(shape[0]);

    for (int frame = 0; frame < 80; frame++) {
        char line[Vfd::MAX_DIGITS + 1];
        for (uint8_t col = 0; col < Vfd::MAX_DIGITS; col++) {
            int index = (col + frame) % shape_len;
            line[col] = levels[shape[index] % num_levels];
        }
        line[Vfd::MAX_DIGITS] = '\0';

        vfd.cursor_move(0);
        vfd.write_string(line);
        if (sleep_or_exit(120)) return true;
    }
    return false;
}

/**
 * @brief A field of diagonal lines that appears to spin/travel across the
 * display, cycling each column through /, |, \ with a phase offset that
 * shifts by one column per frame -- like a traveling barber pole.
 */
bool rotor_effect(Vfd& vfd) {
    vfd.clear_screen();

    static const char glyphs[] = {'/', '|', '\\'};
    const int num_glyphs = sizeof(glyphs) / sizeof(glyphs[0]);

    for (int frame = 0; frame < 90; frame++) {
        char line[Vfd::MAX_DIGITS + 1];
        for (uint8_t col = 0; col < Vfd::MAX_DIGITS; col++) {
            int phase = (col + frame) % num_glyphs;
            line[col] = glyphs[phase];
        }
        line[Vfd::MAX_DIGITS] = '\0';

        vfd.cursor_move(0);
        vfd.write_string(line);
        if (sleep_or_exit(100)) return true;
    }
    return false;
}

typedef bool (*EffectFn)(Vfd&);
constexpr EffectFn effects[] = {scanner_effect, marquee_effect, sparkle_effect, wave_effect, rotor_effect};
constexpr int num_effects = sizeof(effects) / sizeof(effects[0]);

}  // namespace

void run_animation_app(Vfd& vfd) {
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    // Seed with a free-running hardware timer so the effect sequence isn't
    // identical on every boot. Not cryptographic -- just enough variety
    // for a "screensaver".
    srand((unsigned)time_us_64());

    while (true) {
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(1);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        sleep_ms(1);

        if (exit_requested()) {
            printf("-> Exiting Animation, returning to menu\n");
            return;
        }

        int choice = rand() % num_effects;
        bool aborted = effects[choice](vfd);
        vfd.clear_screen();

        if (aborted) {
            printf("-> Exiting Animation, returning to menu\n");
            return;
        }

        if (sleep_or_exit(300)) {
            printf("-> Exiting Animation, returning to menu\n");
            return;
        }
    }
}
