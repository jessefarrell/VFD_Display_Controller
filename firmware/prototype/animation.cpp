#include "animation.h"
#include "app_common.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ---------------------------------------------------------------------------
// Each _effect() function below is self-contained: it clears the screen and
// runs until `duration_ms` has elapsed (checked between "frames" -- a
// sweep position, a scroll tick, a sparkle, etc. -- so a check never lands
// mid-frame). run_animation_app() decides what duration to hand each
// effect: a configurable "shuffle time" when picking effects at random, or
// a very large duration when the user has locked in one specific effect,
// so effects don't need to know *why* they're being asked to run for that
// long, only for how long.
//
// All effects are built purely out of Vfd's public interface (write_char,
// cursor_move, display_scroll, clear_screen) -- no hardware/pin knowledge
// needed here.
//
// Each effect returns true if the user requested an exit (Esc/Ctrl+C)
// mid-effect, via sleep_or_exit(), so run_animation_app() can stop
// immediately instead of waiting for the full duration to elapse.
// ---------------------------------------------------------------------------

namespace {

/**
 * @brief Milliseconds since boot, per the Pico SDK's free-running timer.
 * Used to turn each effect's old fixed frame/pass counts into a time
 * budget instead.
 */
uint32_t _now_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

// Longest custom marquee message accepted from the prompt (not counting
// the null terminator). Generous for a 20-character display, but still a
// fixed cap -- not literally unbounded, given this runs on a Pico.
constexpr size_t MARQUEE_TEXT_MAX_LEN = 1000;

// Blank gap inserted between the end of the message and its repeat, so
// the marquee visibly blanks out for a beat instead of running the last
// and first characters straight into each other.
constexpr const char* MARQUEE_GAP = "    ";

// Current custom marquee text and per-character scroll rate. Set to
// sensible defaults here; overwritten by _configure_marquee() whenever
// the user explicitly selects Marquee from the menu.
char g_marquee_text[MARQUEE_TEXT_MAX_LEN + 1] = "  Schneider Electric  ";
uint32_t g_marquee_rate_ms = 150;

/**
 * @brief Where a snake glyph's left edge picks up, and where its right
 * edge (the new head) leaves off -- what the next glyph must match.
 */
enum class SnakeLevel { TOP, MID, BOTTOM };

struct SnakeGlyph {
    char ch;
    SnakeLevel entry;
    SnakeLevel exit;
};

// '/' and '\' are listed twice on purpose, to bias random picks toward
// them (matching the weighting of the original glyph list). '`' and '\''
// are a two-way bridge between top and mid: mid -> top (climbing back up
// out of a run of '-') and top -> mid (dropping down into one), so mid is
// reachable mid-stream and not just as the rare initial glyph.
constexpr SnakeGlyph snake_glyphs[] = {
    {'/',  SnakeLevel::BOTTOM, SnakeLevel::TOP},
    {'\\', SnakeLevel::TOP,    SnakeLevel::BOTTOM},
    {'~',  SnakeLevel::TOP,    SnakeLevel::TOP},
    {'/',  SnakeLevel::BOTTOM, SnakeLevel::TOP},
    // {'\\', SnakeLevel::TOP,    SnakeLevel::BOTTOM},
    {'^',  SnakeLevel::BOTTOM, SnakeLevel::BOTTOM},
    {'_',  SnakeLevel::BOTTOM, SnakeLevel::BOTTOM},
    // {'-',  SnakeLevel::MID,    SnakeLevel::MID},
    // {'`',  SnakeLevel::MID,    SnakeLevel::TOP},
    // {'\'', SnakeLevel::MID,    SnakeLevel::TOP},
    // {'`',  SnakeLevel::TOP,    SnakeLevel::MID},
    // {'\'', SnakeLevel::TOP,    SnakeLevel::MID},
};
constexpr int num_snake_glyphs = sizeof(snake_glyphs) / sizeof(snake_glyphs[0]);

/**
 * @brief Picks the very first glyph of a fresh snake, when there's no
 * prior head position to continue from -- so any glyph is fair game.
 */
char _pick_initial_snake_glyph(SnakeLevel* out_level) {
    const SnakeGlyph& g = snake_glyphs[rand() % num_snake_glyphs];
    *out_level = g.exit;
    return g.ch;
}

/**
 * @brief Picks the next glyph given the current head position, choosing
 * uniformly among every glyph whose entry matches it. The table above is
 * built so this set is never empty for any SnakeLevel.
 */
char _pick_next_snake_glyph(SnakeLevel current_level, SnakeLevel* out_level) {
    char candidates[num_snake_glyphs];
    SnakeLevel candidate_exits[num_snake_glyphs];
    int count = 0;

    for (int i = 0; i < num_snake_glyphs; i++) {
        if (snake_glyphs[i].entry == current_level) {
            candidates[count] = snake_glyphs[i].ch;
            candidate_exits[count] = snake_glyphs[i].exit;
            count++;
        }
    }

    int choice = rand() % count;
    *out_level = candidate_exits[choice];
    return candidates[choice];
}

/**
 * @brief Knight-Rider style scanner: a single character bounces back and
 * forth across the full width of the display, repeating sweeps until
 * duration_ms has elapsed.
 */
bool scanner_effect(Vfd& vfd, uint32_t duration_ms) {
    vfd.clear_screen();
    const uint8_t last = Vfd::MAX_DIGITS - 1;
    uint32_t start = _now_ms();

    while (_now_ms() - start < duration_ms) {
        for (uint8_t pos = 0; pos <= last; pos++) {
            if (_now_ms() - start >= duration_ms) return false;
            vfd.cursor_move(pos);
            vfd.write_char('#');
            if (sleep_or_exit(60)) return true;
            vfd.cursor_move(pos);
            vfd.write_char(' ');
        }
        for (int pos = last; pos >= 0; pos--) {
            if (_now_ms() - start >= duration_ms) return false;
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
 * @brief Scrolling marquee: renders a sliding Vfd::MAX_DIGITS-wide window
 * over g_marquee_text (plus a blank gap), advancing one character per
 * tick at g_marquee_rate_ms, until duration_ms has elapsed.
 *
 * Note this does NOT use Vfd::display_scroll() -- that only rotates
 * whatever's already sitting in the display's own 20-character buffer,
 * which can't hold a message longer than the screen. Instead this builds
 * its own (much larger) source buffer here and redraws a fresh 20-char
 * window from it every tick, the same full-line-redraw technique wave_effect
 * and rotor_effect use.
 */
bool marquee_effect(Vfd& vfd, uint32_t duration_ms) {
    vfd.clear_screen();

    char source[MARQUEE_TEXT_MAX_LEN + 8];
    std::snprintf(source, sizeof(source), "%s%s", g_marquee_text, MARQUEE_GAP);
    size_t source_len = strlen(source);
    if (source_len == 0) {
        // Nothing to show; just idle until duration elapses (or an exit
        // is requested).
        return sleep_or_exit(duration_ms);
    }

    char window[Vfd::MAX_DIGITS + 1];
    uint32_t start = _now_ms();
    size_t offset = 0;

    while (_now_ms() - start < duration_ms) {
        for (uint8_t i = 0; i < Vfd::MAX_DIGITS; i++) {
            window[i] = source[(offset + i) % source_len];
        }
        window[Vfd::MAX_DIGITS] = '\0';

        vfd.cursor_move(0);
        vfd.write_string(window);
        if (sleep_or_exit(g_marquee_rate_ms)) return true;

        offset = (offset + 1) % source_len;
    }
    return false;
}

/**
 * @brief Random twinkling characters, like a field of stars, until
 * duration_ms has elapsed.
 */
bool sparkle_effect(Vfd& vfd, uint32_t duration_ms) {
    vfd.clear_screen();
    static const char sparkle_chars[] = {'*', '.', '+', 'o', '\''};
    const int num_chars = sizeof(sparkle_chars) / sizeof(sparkle_chars[0]);

    uint32_t start = _now_ms();
    int frame = 0;
    while (_now_ms() - start < duration_ms) {
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
        frame++;
    }
    return false;
}

/**
 * @brief A rolling wave that travels across the display, built from a
 * small fixed "amplitude" shape rather than floating-point trig -- keeps
 * this effect dependency-free. Runs until duration_ms has elapsed.
 */
bool wave_effect(Vfd& vfd, uint32_t duration_ms) {
    vfd.clear_screen();

    static const char levels[] = {'.', '-', '=', '#', '=', '-'};
    const int num_levels = sizeof(levels) / sizeof(levels[0]);

    // One full up-and-down cycle of the wave, expressed as indices into
    // `levels`. Repeating/offsetting this against each column position
    // is what makes the wave appear to travel.
    static const int shape[] = {0, 1, 2, 3, 4, 5, 5, 4, 3, 2, 1, 0};
    const int shape_len = sizeof(shape) / sizeof(shape[0]);

    uint32_t start = _now_ms();
    int frame = 0;
    while (_now_ms() - start < duration_ms) {
        char line[Vfd::MAX_DIGITS + 1];
        for (uint8_t col = 0; col < Vfd::MAX_DIGITS; col++) {
            int index = (col + frame) % shape_len;
            line[col] = levels[shape[index] % num_levels];
        }
        line[Vfd::MAX_DIGITS] = '\0';

        vfd.cursor_move(0);
        vfd.write_string(line);
        if (sleep_or_exit(120)) return true;
        frame++;
    }
    return false;
}

/**
 * @brief A field of diagonal lines that appears to spin/travel across the
 * display, cycling each column through a small glyph set with a phase
 * offset that shifts by one column per frame -- like a traveling barber
 * pole. Runs until duration_ms has elapsed.
 */
bool rotor_effect(Vfd& vfd, uint32_t duration_ms) {
    vfd.clear_screen();

    static const char glyphs[] = {'/','<','\\','~','/','>','\\','^'};
    const int num_glyphs = sizeof(glyphs) / sizeof(glyphs[0]);

    uint32_t start = _now_ms();
    int frame = 0;
    while (_now_ms() - start < duration_ms) {
        char line[Vfd::MAX_DIGITS + 1];
        for (uint8_t col = 0; col < Vfd::MAX_DIGITS; col++) {
            int phase = (col + frame) % num_glyphs;
            line[col] = glyphs[phase];
        }
        line[Vfd::MAX_DIGITS] = '\0';

        vfd.cursor_move(0);
        vfd.write_string(line);
        if (sleep_or_exit(100)) return true;
        frame++;
    }
    return false;
}

/**
 * @brief A snake built from diagonal/flat glyphs that traverses the
 * display left to right, each new glyph continuing smoothly from where
 * the previous one left off (see SnakeGlyph table above). Growth phase:
 * reveals one glyph at a time until the full width is covered. Flow
 * phase: once full, keeps generating new glyphs on the right while the
 * oldest drops off the left, for the rest of duration_ms.
 */
bool snake_effect(Vfd& vfd, uint32_t duration_ms) {
    vfd.clear_screen();

    char buf[Vfd::MAX_DIGITS + 1];
    memset(buf, ' ', Vfd::MAX_DIGITS);
    buf[Vfd::MAX_DIGITS] = '\0';

    uint32_t start = _now_ms();
    SnakeLevel head_level;

    // Growth phase.
    int len = 0;
    for (; len < Vfd::MAX_DIGITS; len++) {
        buf[len] = (len == 0) ? _pick_initial_snake_glyph(&head_level)
                               : _pick_next_snake_glyph(head_level, &head_level);

        vfd.cursor_move(0);
        vfd.write_string(buf);
        if (sleep_or_exit(150)) return true;
        if (_now_ms() - start >= duration_ms) return false;
    }

    // Flow phase: shift the buffer left, dropping the oldest segment, and
    // grow a new one onto the tail -- the snake looks like it keeps
    // crawling rightward even though the display itself is static text.
    while (_now_ms() - start < duration_ms) {
        char c = _pick_next_snake_glyph(head_level, &head_level);
        memmove(buf, buf + 1, Vfd::MAX_DIGITS - 1);
        buf[Vfd::MAX_DIGITS - 1] = c;

        vfd.cursor_move(0);
        vfd.write_string(buf);
        if (sleep_or_exit(150)) return true;
    }
    return false;
}

// Forward-declared: defined further down alongside the other prompt
// helpers, but referenced by the effects[] table right below.
void _configure_marquee(void);

typedef bool (*EffectFn)(Vfd&, uint32_t);
typedef void (*ConfigureFn)(void);

struct Effect {
    const char* name;
    EffectFn fn;
    // Optional: called once, right after this effect is explicitly
    // selected from the menu (not when it comes up during Random
    // rotation), to prompt for any settings it needs. nullptr if the
    // effect takes no configuration.
    ConfigureFn configure;
};

constexpr Effect effects[] = {
    {"Scanner", scanner_effect, nullptr},
    {"Marquee", marquee_effect, _configure_marquee},
    {"Sparkle", sparkle_effect, nullptr},
    {"Wave",    wave_effect,    nullptr},
    {"Rotor",   rotor_effect,   nullptr},
    {"Snake",   snake_effect,   nullptr},
};
constexpr int num_effects = sizeof(effects) / sizeof(effects[0]);

// current_effect_index of -1 means "random" (a new effect is picked each
// pass through the loop, each played for shuffle_ms).
constexpr int RANDOM_EFFECT = -1;

// Default duration each effect plays for in random/shuffle mode, until the
// user overrides it at the prompt. Roughly matches how long the old
// fixed-frame-count effects used to run.
constexpr uint32_t DEFAULT_SHUFFLE_MS = 8000;

// Duration used when the user has locked in one specific effect. Not
// literally infinite -- effects still poll for exit/switch keys against
// this budget -- just long enough that in practice it plays continuously
// until the user switches or exits.
constexpr uint32_t SINGLE_EFFECT_DURATION_MS = 3600000UL;  // 1 hour

/**
 * @brief Blocking read of one character from stdin, echoed back to console.
 *
 * Mirrors the read_char() helper in main.cpp / test_display.cpp -- kept
 * local here rather than shared so this file stays self-contained.
 */
char read_char(void) {
    int c = getchar_timeout_us(UINT32_MAX);  // block indefinitely
    if (c == PICO_ERROR_TIMEOUT) {
        return '\0';  // shouldn't happen with UINT32_MAX, but just in case
    }
    putchar((char)c);
    return (char)c;
}

/**
 * @brief Blocking read of a short digits-only line from stdin, terminated
 * by Enter. Backspace/Delete removes the last digit. Esc or Ctrl+C aborts
 * the entry (e.g. if the user changes their mind mid-prompt).
 *
 * @param out_value - on success, set to the parsed value.
 * @return true if a non-empty value was entered and parsed; false if the
 * user aborted or entered nothing, in which case *out_value is untouched.
 */
bool _read_uint_line(uint32_t* out_value) {
    char buf[8] = {0};
    size_t len = 0;

    while (true) {
        char c = read_char();

        if (is_exit_key(c)) {
            printf("\n");
            return false;
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
        return false;
    }
    *out_value = (uint32_t)atoi(buf);
    return true;
}

/**
 * @brief Prompts for a shuffle time in seconds and returns it in
 * milliseconds. Pressing Enter with no digits, or Esc/Ctrl+C, keeps
 * current_ms unchanged.
 */
uint32_t _prompt_shuffle_time_ms(uint32_t current_ms) {
    printf("Shuffle time in seconds (Enter to keep %lus): ",
           (unsigned long)(current_ms / 1000));

    uint32_t seconds;
    if (!_read_uint_line(&seconds) || seconds == 0) {
        return current_ms;
    }
    return seconds * 1000;
}

/**
 * @brief Blocking read of a line of printable text from stdin, terminated
 * by Enter. Backspace/Delete removes the last character. Esc or Ctrl+C
 * aborts the entry.
 *
 * @param buf - destination buffer; always null-terminated on return.
 * @param buf_size - size of buf, including room for the null terminator.
 * @return true if Enter was pressed (buf holds whatever was typed, which
 * may be empty); false if the user aborted with Esc/Ctrl+C, in which case
 * buf is left untouched.
 */
bool _read_text_line(char* buf, size_t buf_size) {
    size_t len = 0;
    buf[0] = '\0';

    while (true) {
        char c = read_char();

        if (is_exit_key(c)) {
            printf("\n");
            return false;
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
        if (c >= 0x20 && c < 0x7F && len + 1 < buf_size) {  // printable ASCII
            buf[len++] = c;
            buf[len] = '\0';
        }
        // Non-printable input beyond that, or input past buf_size, is
        // silently ignored.
    }
    return true;
}

/**
 * @brief Prompts for the marquee's display text. Enter with nothing
 * typed, or Esc/Ctrl+C, leaves text_buf unchanged.
 */
void _prompt_marquee_text(char* text_buf, size_t buf_size) {
    printf("Marquee text (Enter to keep \"%s\"): ", text_buf);

    char entered[MARQUEE_TEXT_MAX_LEN + 1];
    if (_read_text_line(entered, sizeof(entered)) && entered[0] != '\0') {
        strncpy(text_buf, entered, buf_size - 1);
        text_buf[buf_size - 1] = '\0';
    }
}

/**
 * @brief Prompts for the marquee's per-character update rate in
 * milliseconds -- how often a new character is pulled in on one edge and
 * pushed out the other. Enter with no digits, or Esc/Ctrl+C, keeps
 * current_ms unchanged.
 */
uint32_t _prompt_marquee_rate_ms(uint32_t current_ms) {
    printf("Update rate in ms/character (Enter to keep %lums): ", (unsigned long)current_ms);

    uint32_t ms;
    if (!_read_uint_line(&ms) || ms == 0) {
        return current_ms;
    }
    return ms;
}

/**
 * @brief Pre-selection prompt hook for Marquee: asks for the display
 * text and update rate before playback starts.
 */
void _configure_marquee(void) {
    _prompt_marquee_text(g_marquee_text, sizeof(g_marquee_text));
    g_marquee_rate_ms = _prompt_marquee_rate_ms(g_marquee_rate_ms);
}

void print_animation_menu(void) {
    printf("\n--- Animation Selector ---\n");
    for (int i = 0; i < num_effects; i++) {
        if (effects[i].configure) {
            printf("  [%d] %s (prompts for settings)\n", i + 1, effects[i].name);
        } else {
            printf("  [%d] %s\n", i + 1, effects[i].name);
        }
    }
    printf("  [0] Random (cycles through all effects, one \"shuffle time\" each)\n");
    printf("You can press a number at any time during playback to switch.\n");
    printf("Ctrl+A replays this menu. Esc / Ctrl+C exits to the main menu.\n");
    printf("---------------------------\n");
    printf("Select: ");
}

/**
 * @brief Parses a menu keypress into an effect index.
 *
 * @param c - key pressed ('0'-'5' expected; anything else is ignored)
 * @param out_index - set to RANDOM_EFFECT for '0', or the matching effect
 * index for '1'-'0'+num_effects.
 * @return true if c mapped to a valid selection.
 */
bool _parse_effect_selection(char c, int& out_index) {
    if (c == '0') {
        out_index = RANDOM_EFFECT;
        return true;
    }
    if (c >= '1' && c <= '0' + num_effects) {
        out_index = (c - '1');
        return true;
    }
    return false;
}

}  // namespace

void run_animation_app(Vfd& vfd) {
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    // Seed with a free-running hardware timer so the random effect
    // sequence isn't identical on every boot. Not cryptographic -- just
    // enough variety for a "screensaver".
    srand((unsigned)time_us_64());

    int current_effect = RANDOM_EFFECT;
    uint32_t shuffle_ms = DEFAULT_SHUFFLE_MS;

    print_animation_menu();
    char choice = read_char();
    printf("\n");

    if (is_exit_key(choice)) {
        printf("-> Exiting Animation, returning to menu\n");
        return;
    }
    if (_parse_effect_selection(choice, current_effect)) {
        if (current_effect == RANDOM_EFFECT) {
            shuffle_ms = _prompt_shuffle_time_ms(shuffle_ms);
            printf("-> Playing effects at random, %lus each\n", (unsigned long)(shuffle_ms / 1000));
        } else {
            if (effects[current_effect].configure) {
                effects[current_effect].configure();
            }
            printf("-> Running %s on repeat\n", effects[current_effect].name);
        }
    } else {
        printf("-> Unrecognized selection, defaulting to Random\n");
        current_effect = RANDOM_EFFECT;
        shuffle_ms = _prompt_shuffle_time_ms(shuffle_ms);
    }

    while (true) {
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(1);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        sleep_ms(1);

        // Check for a menu key between effects: Esc/Ctrl+C exits, Ctrl+A
        // reprints the menu, and a digit switches effects live without
        // interrupting playback.
        int key = poll_char();
        if (key != -1) {
            char c = (char)key;
            if (is_exit_key(c)) {
                printf("-> Exiting Animation, returning to menu\n");
                return;
            } else if (c == 0x01) {  // Ctrl+A
                print_animation_menu();
            } else {
                int selected;
                if (_parse_effect_selection(c, selected)) {
                    current_effect = selected;
                    if (current_effect == RANDOM_EFFECT) {
                        shuffle_ms = _prompt_shuffle_time_ms(shuffle_ms);
                        printf("\n-> Playing effects at random, %lus each\n", (unsigned long)(shuffle_ms / 1000));
                    } else {
                        if (effects[current_effect].configure) {
                            effects[current_effect].configure();
                        }
                        printf("\n-> %s\n", effects[current_effect].name);
                    }
                }
            }
        }

        uint32_t duration_ms = (current_effect == RANDOM_EFFECT) ? shuffle_ms : SINGLE_EFFECT_DURATION_MS;
        int index = (current_effect == RANDOM_EFFECT) ? (rand() % num_effects) : current_effect;
        bool aborted = effects[index].fn(vfd, duration_ms);
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
